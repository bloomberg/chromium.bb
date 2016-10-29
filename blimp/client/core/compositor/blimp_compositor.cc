// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/compositor/blimp_compositor.h"

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "blimp/client/core/compositor/blimp_compositor_dependencies.h"
#include "blimp/client/core/compositor/blimp_compositor_frame_sink.h"
#include "blimp/client/public/compositor/compositor_dependencies.h"
#include "blimp/net/blimp_stats.h"
#include "cc/animation/animation_host.h"
#include "cc/blimp/client_picture_cache.h"
#include "cc/blimp/compositor_state_deserializer.h"
#include "cc/blimp/image_serialization_processor.h"
#include "cc/layers/layer.h"
#include "cc/layers/surface_layer.h"
#include "cc/output/compositor_frame_sink.h"
#include "cc/proto/compositor_message.pb.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/trees/layer_tree_host_in_process.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "net/base/net_errors.h"
#include "ui/gl/gl_surface.h"

namespace blimp {
namespace client {

namespace {

void SatisfyCallback(cc::SurfaceManager* manager,
                     const cc::SurfaceSequence& sequence) {
  std::vector<uint32_t> sequences;
  sequences.push_back(sequence.sequence);
  manager->DidSatisfySequences(sequence.frame_sink_id, &sequences);
}

void RequireCallback(cc::SurfaceManager* manager,
                     const cc::SurfaceId& id,
                     const cc::SurfaceSequence& sequence) {
  cc::Surface* surface = manager->GetSurfaceForId(id);
  if (!surface) {
    LOG(ERROR) << "Attempting to require callback on nonexistent surface";
    return;
  }
  surface->AddDestructionDependency(sequence);
}

}  // namespace

BlimpCompositor::BlimpCompositor(
    BlimpCompositorDependencies* compositor_dependencies,
    BlimpCompositorClient* client,
    bool use_threaded_layer_tree_host)
    : use_threaded_layer_tree_host_(use_threaded_layer_tree_host),
      client_(client),
      compositor_dependencies_(compositor_dependencies),
      frame_sink_id_(compositor_dependencies_->GetEmbedderDependencies()
                         ->AllocateFrameSinkId()),
      proxy_client_(nullptr),
      compositor_frame_sink_request_pending_(false),
      layer_(cc::Layer::Create()),
      remote_proto_channel_receiver_(nullptr),
      outstanding_commits_(0U),
      weak_ptr_factory_(this) {
  DCHECK(thread_checker_.CalledOnValidThread());

  surface_id_allocator_ = base::MakeUnique<cc::SurfaceIdAllocator>();
  GetEmbedderDeps()->GetSurfaceManager()->RegisterFrameSinkId(frame_sink_id_);
  CreateLayerTreeHost();

  if (use_threaded_layer_tree_host_) {
    std::unique_ptr<cc::ClientPictureCache> client_picture_cache =
        compositor_dependencies_->GetImageSerializationProcessor()
            ->CreateClientPictureCache();
    compositor_state_deserializer_ =
        base::MakeUnique<cc::CompositorStateDeserializer>(
            host_.get(), std::move(client_picture_cache),
            base::Bind(&BlimpCompositor::LayerScrolled,
                       weak_ptr_factory_.GetWeakPtr()),
            this);
  }
}

BlimpCompositor::~BlimpCompositor() {
  DCHECK(thread_checker_.CalledOnValidThread());

  DestroyLayerTreeHost();
  GetEmbedderDeps()->GetSurfaceManager()->InvalidateFrameSinkId(frame_sink_id_);

  CheckPendingCommitCounts(true /* flush */);
}

void BlimpCompositor::SetVisible(bool visible) {
  host_->SetVisible(visible);

  if (!visible)
    CheckPendingCommitCounts(true /* flush */);
}

void BlimpCompositor::NotifyWhenDonePendingCommits(base::Closure callback) {
  if (outstanding_commits_ == 0) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
    return;
  }

  pending_commit_trackers_.push_back(
      std::make_pair(outstanding_commits_, callback));
}

void BlimpCompositor::UpdateLayerTreeHost() {
  if (pending_frame_update_) {
    DCHECK(use_threaded_layer_tree_host_);
    compositor_state_deserializer_->DeserializeCompositorUpdate(
        pending_frame_update_->layer_tree_host());
    pending_frame_update_ = nullptr;
    cc::proto::CompositorMessage frame_ack;
    frame_ack.set_frame_ack(true);
    client_->SendCompositorMessage(frame_ack);
  }
}

void BlimpCompositor::RequestNewCompositorFrameSink() {
  DCHECK(!surface_factory_);
  DCHECK(!compositor_frame_sink_request_pending_);

  compositor_frame_sink_request_pending_ = true;
  GetEmbedderDeps()->GetContextProviders(
      base::Bind(&BlimpCompositor::OnContextProvidersCreated,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BlimpCompositor::DidInitializeCompositorFrameSink() {
  compositor_frame_sink_request_pending_ = false;
}

void BlimpCompositor::DidCommitAndDrawFrame() {
  BlimpStats::GetInstance()->Add(BlimpStats::COMMIT, 1);

  DCHECK_GT(outstanding_commits_, 0U);
  outstanding_commits_--;

  CheckPendingCommitCounts(false /* flush */);
}

void BlimpCompositor::SetProtoReceiver(ProtoReceiver* receiver) {
  DCHECK(!use_threaded_layer_tree_host_);
  remote_proto_channel_receiver_ = receiver;
}

void BlimpCompositor::SendCompositorProto(
    const cc::proto::CompositorMessage& proto) {
  DCHECK(!use_threaded_layer_tree_host_);
  client_->SendCompositorMessage(proto);
}

void BlimpCompositor::OnCompositorMessageReceived(
    std::unique_ptr<cc::proto::CompositorMessage> message) {
  if (message->has_to_impl()) {
    HandleCompositorMessageToImpl(std::move(message));
    return;
  }

  DCHECK(use_threaded_layer_tree_host_);
  DCHECK(message->has_layer_tree_host())
      << "The engine only sends frame updates";
  DCHECK(!pending_frame_update_)
      << "We should have only a single frame in flight";

  UMA_HISTOGRAM_MEMORY_KB("Blimp.Compositor.CommitSizeKb",
                          (float)message->ByteSize() / 1024);
  pending_frame_update_ = std::move(message);
  outstanding_commits_++;
  host_->SetNeedsAnimate();
}

void BlimpCompositor::HandleCompositorMessageToImpl(
    std::unique_ptr<cc::proto::CompositorMessage> message) {
  DCHECK(!use_threaded_layer_tree_host_);
  DCHECK(message->has_to_impl());

  const cc::proto::CompositorMessageToImpl to_impl_proto = message->to_impl();
  DCHECK(to_impl_proto.has_message_type());

  if (to_impl_proto.message_type() ==
      cc::proto::CompositorMessageToImpl::START_COMMIT) {
    outstanding_commits_++;
  }

  switch (to_impl_proto.message_type()) {
    case cc::proto::CompositorMessageToImpl::UNKNOWN:
      NOTIMPLEMENTED() << "Ignoring message of UNKNOWN type";
      break;
    case cc::proto::CompositorMessageToImpl::START_COMMIT:
      UMA_HISTOGRAM_MEMORY_KB("Blimp.Compositor.CommitSizeKb",
                              (float)message->ByteSize() / 1024);
    default:
      // We should have a receiver if we're getting compositor messages that
      // are not INITIALIZE_IMPL or CLOSE_IMPL.
      DCHECK(remote_proto_channel_receiver_);
      remote_proto_channel_receiver_->OnProtoReceived(std::move(message));
  }
}

// Returns a reference to the InputHandler owned by layer tree host.
const base::WeakPtr<cc::InputHandler>& BlimpCompositor::GetInputHandler() {
  return host_->GetInputHandler();
}

void BlimpCompositor::OnContextProvidersCreated(
    const scoped_refptr<cc::ContextProvider>& compositor_context_provider,
    const scoped_refptr<cc::ContextProvider>& worker_context_provider) {
  DCHECK(!surface_factory_) << "Any connection to the old CompositorFrameSink "
                               "should have been destroyed";

  // Make sure we still have a host and we're still expecting a
  // CompositorFrameSink. This can happen if the host dies while the request is
  // outstanding and we build a new one that hasn't asked for a surface yet.
  if (!compositor_frame_sink_request_pending_)
    return;

  // Try again if the context creation failed.
  if (!compositor_context_provider) {
    GetEmbedderDeps()->GetContextProviders(
        base::Bind(&BlimpCompositor::OnContextProvidersCreated,
                   weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  auto compositor_frame_sink = base::MakeUnique<BlimpCompositorFrameSink>(
      std::move(compositor_context_provider),
      std::move(worker_context_provider),
      GetEmbedderDeps()->GetGpuMemoryBufferManager(), nullptr,
      base::ThreadTaskRunnerHandle::Get(), weak_ptr_factory_.GetWeakPtr());

  host_->SetCompositorFrameSink(std::move(compositor_frame_sink));
}

void BlimpCompositor::BindToProxyClient(
    base::WeakPtr<BlimpCompositorFrameSinkProxyClient> proxy_client) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!surface_factory_);

  proxy_client_ = proxy_client;
  surface_factory_ = base::MakeUnique<cc::SurfaceFactory>(
      frame_sink_id_, GetEmbedderDeps()->GetSurfaceManager(), this);
}

void BlimpCompositor::SubmitCompositorFrame(cc::CompositorFrame frame) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(surface_factory_);

  cc::RenderPass* root_pass =
      frame.delegated_frame_data->render_pass_list.back().get();
  gfx::Size surface_size = root_pass->output_rect.size();

  if (local_frame_id_.is_null() || current_surface_size_ != surface_size) {
    DestroyDelegatedContent();
    DCHECK(layer_->children().empty());

    local_frame_id_ = surface_id_allocator_->GenerateId();
    surface_factory_->Create(local_frame_id_);
    current_surface_size_ = surface_size;

    // manager must outlive compositors using it.
    cc::SurfaceManager* surface_manager =
        GetEmbedderDeps()->GetSurfaceManager();
    scoped_refptr<cc::SurfaceLayer> content_layer = cc::SurfaceLayer::Create(
        base::Bind(&SatisfyCallback, base::Unretained(surface_manager)),
        base::Bind(&RequireCallback, base::Unretained(surface_manager)));
    content_layer->SetSurfaceId(
        cc::SurfaceId(surface_factory_->frame_sink_id(), local_frame_id_), 1.f,
        surface_size);
    content_layer->SetBounds(current_surface_size_);
    content_layer->SetIsDrawable(true);
    content_layer->SetContentsOpaque(true);

    layer_->AddChild(content_layer);
  }

  surface_factory_->SubmitCompositorFrame(
      local_frame_id_, std::move(frame),
      base::Bind(&BlimpCompositor::SubmitCompositorFrameAck,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BlimpCompositor::SubmitCompositorFrameAck() {
  DCHECK(surface_factory_);
  compositor_dependencies_->GetCompositorTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&BlimpCompositorFrameSinkProxyClient::SubmitCompositorFrameAck,
                 proxy_client_));
}

void BlimpCompositor::UnbindProxyClient() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(surface_factory_);

  DestroyDelegatedContent();
  surface_factory_.reset();
  proxy_client_ = nullptr;
}

void BlimpCompositor::ReturnResources(
    const cc::ReturnedResourceArray& resources) {
  DCHECK(surface_factory_);
  compositor_dependencies_->GetCompositorTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(
          &BlimpCompositorFrameSinkProxyClient::ReclaimCompositorResources,
          proxy_client_, resources));
}

bool BlimpCompositor::ShouldRetainClientScroll(
    int engine_layer_id,
    const gfx::ScrollOffset& new_offset) {
  // TODO(khushalsagar): Update when adding scroll/scale sync. See
  // crbug.com/648442.
  return true;
}

bool BlimpCompositor::ShouldRetainClientPageScale(float new_page_scale) {
  // TODO(khushalsagar): Update when adding scroll/scale sync. See
  // crbug.com/648442.
  return true;
}

void BlimpCompositor::LayerScrolled(int engine_layer_id) {
  DCHECK(use_threaded_layer_tree_host_);
}

CompositorDependencies* BlimpCompositor::GetEmbedderDeps() {
  return compositor_dependencies_->GetEmbedderDependencies();
}

void BlimpCompositor::DestroyDelegatedContent() {
  if (local_frame_id_.is_null())
    return;

  // Remove any references for the surface layer that uses this
  // |local_frame_id_|.
  layer_->RemoveAllChildren();
  surface_factory_->Destroy(local_frame_id_);
  local_frame_id_ = cc::LocalFrameId();
}

void BlimpCompositor::CreateLayerTreeHost() {
  DCHECK(!host_);
  VLOG(1) << "Creating LayerTreeHost.";

  // Create the LayerTreeHost
  cc::LayerTreeHostInProcess::InitParams params;
  params.client = this;
  params.task_graph_runner = compositor_dependencies_->GetTaskGraphRunner();
  params.main_task_runner = base::ThreadTaskRunnerHandle::Get();
  if (!use_threaded_layer_tree_host_) {
    params.image_serialization_processor =
        compositor_dependencies_->GetImageSerializationProcessor();
  }

  cc::LayerTreeSettings* settings =
      compositor_dependencies_->GetLayerTreeSettings();
  params.settings = settings;

  params.animation_host = cc::AnimationHost::CreateMainInstance();

  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner =
      compositor_dependencies_->GetCompositorTaskRunner();

  if (use_threaded_layer_tree_host_) {
    host_ = cc::LayerTreeHostInProcess::CreateThreaded(compositor_task_runner,
                                                       &params);
  } else {
    host_ = cc::LayerTreeHostInProcess::CreateRemoteClient(
        this /* remote_proto_channel */, compositor_task_runner, &params);
  }
}

void BlimpCompositor::DestroyLayerTreeHost() {
  DCHECK(host_);
  VLOG(1) << "Destroying LayerTreeHost.";

  // Tear down the output surface connection with the old LayerTreeHost
  // instance.
  DestroyDelegatedContent();
  surface_factory_.reset();

  // Destroy the old LayerTreeHost state.
  host_.reset();

  // Cancel any outstanding CompositorFrameSink requests.  That way if we get an
  // async callback related to the old request we know to drop it.
  compositor_frame_sink_request_pending_ = false;

  // Make sure we don't have a receiver at this point.
  DCHECK(!remote_proto_channel_receiver_);
}

void BlimpCompositor::CheckPendingCommitCounts(bool flush) {
  for (auto it = pending_commit_trackers_.begin();
       it != pending_commit_trackers_.end();) {
    if (flush || --it->first == 0) {
      it->second.Run();
      it = pending_commit_trackers_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace client
}  // namespace blimp
