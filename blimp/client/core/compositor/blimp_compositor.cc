// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/compositor/blimp_compositor.h"

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "blimp/client/core/compositor/blimp_compositor_dependencies.h"
#include "blimp/client/core/compositor/delegated_output_surface.h"
#include "blimp/client/public/compositor/compositor_dependencies.h"
#include "blimp/net/blimp_stats.h"
#include "cc/animation/animation_host.h"
#include "cc/layers/layer.h"
#include "cc/layers/surface_layer.h"
#include "cc/output/output_surface.h"
#include "cc/proto/compositor_message.pb.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "cc/surfaces/surface_manager.h"
#include "cc/trees/layer_tree_host.h"
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
  manager->DidSatisfySequences(sequence.client_id, &sequences);
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
    int render_widget_id,
    BlimpCompositorDependencies* compositor_dependencies,
    BlimpCompositorClient* client)
    : render_widget_id_(render_widget_id),
      client_(client),
      compositor_dependencies_(compositor_dependencies),
      host_should_be_visible_(false),
      output_surface_(nullptr),
      output_surface_request_pending_(false),
      layer_(cc::Layer::Create()),
      remote_proto_channel_receiver_(nullptr),
      weak_ptr_factory_(this) {
  DCHECK(thread_checker_.CalledOnValidThread());

  surface_id_allocator_ = base::MakeUnique<cc::SurfaceIdAllocator>(
      GetEmbedderDeps()->AllocateSurfaceId());
  GetEmbedderDeps()->GetSurfaceManager()->RegisterSurfaceClientId(
      surface_id_allocator_->client_id());
}

BlimpCompositor::~BlimpCompositor() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (host_)
    DestroyLayerTreeHost();

  GetEmbedderDeps()->GetSurfaceManager()->InvalidateSurfaceClientId(
      surface_id_allocator_->client_id());
}

void BlimpCompositor::SetVisible(bool visible) {
  host_should_be_visible_ = visible;
  if (host_)
    host_->SetVisible(host_should_be_visible_);
}

bool BlimpCompositor::OnTouchEvent(const ui::MotionEvent& motion_event) {
  if (input_manager_)
    return input_manager_->OnTouchEvent(motion_event);
  return false;
}

void BlimpCompositor::RequestNewOutputSurface() {
  DCHECK(!surface_factory_);
  DCHECK(!output_surface_request_pending_);

  output_surface_request_pending_ = true;
  GetEmbedderDeps()->GetContextProvider(
      base::Bind(&BlimpCompositor::OnContextProviderCreated,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BlimpCompositor::DidInitializeOutputSurface() {
  output_surface_request_pending_ = false;
}

void BlimpCompositor::DidCommitAndDrawFrame() {
  BlimpStats::GetInstance()->Add(BlimpStats::COMMIT, 1);
}

void BlimpCompositor::SetProtoReceiver(ProtoReceiver* receiver) {
  remote_proto_channel_receiver_ = receiver;
}

void BlimpCompositor::SendCompositorProto(
    const cc::proto::CompositorMessage& proto) {
  client_->SendCompositorMessage(render_widget_id_, proto);
}

void BlimpCompositor::OnCompositorMessageReceived(
    std::unique_ptr<cc::proto::CompositorMessage> message) {
  DCHECK(message->has_to_impl());
  const cc::proto::CompositorMessageToImpl& to_impl_proto = message->to_impl();

  DCHECK(to_impl_proto.has_message_type());
  switch (to_impl_proto.message_type()) {
    case cc::proto::CompositorMessageToImpl::UNKNOWN:
      NOTIMPLEMENTED() << "Ignoring message of UNKNOWN type";
      break;
    case cc::proto::CompositorMessageToImpl::INITIALIZE_IMPL:
      DCHECK(!host_);
      DCHECK(to_impl_proto.has_initialize_impl_message());

      // Create the remote client LayerTreeHost for the compositor.
      CreateLayerTreeHost(to_impl_proto.initialize_impl_message());
      break;
    case cc::proto::CompositorMessageToImpl::CLOSE_IMPL:
      DCHECK(host_);

      // Destroy the remote client LayerTreeHost for the compositor.
      DestroyLayerTreeHost();
      break;
    default:
      // We should have a receiver if we're getting compositor messages that
      // are not INITIALIZE_IMPL or CLOSE_IMPL.
      DCHECK(remote_proto_channel_receiver_);
      remote_proto_channel_receiver_->OnProtoReceived(std::move(message));
  }
}

void BlimpCompositor::OnContextProviderCreated(
    const scoped_refptr<cc::ContextProvider>& provider) {
  DCHECK(!surface_factory_)
      << "Any connection to the old output surface should have been destroyed";

  // Make sure we still have a host and we're still expecting an output surface.
  // This can happen if the host dies while the request is outstanding and we
  // build a new one that hasn't asked for a surface yet.
  if (!output_surface_request_pending_)
    return;

  // TODO(khushalsagar): Make a worker context and bind it to the current
  // thread:
  // Worker context is bound to the main thread in RenderThreadImpl. One day
  // that will change and then this will have to be removed.
  // worker_context_provider->BindToCurrentThread();
  std::unique_ptr<DelegatedOutputSurface> delegated_output_surface =
      base::MakeUnique<DelegatedOutputSurface>(
          provider, nullptr, base::ThreadTaskRunnerHandle::Get(),
          weak_ptr_factory_.GetWeakPtr());

  host_->SetOutputSurface(std::move(delegated_output_surface));
}

void BlimpCompositor::SendWebGestureEvent(
    const blink::WebGestureEvent& gesture_event) {
  client_->SendWebGestureEvent(render_widget_id_, gesture_event);
}

void BlimpCompositor::BindToOutputSurface(
    base::WeakPtr<BlimpOutputSurface> output_surface) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!surface_factory_);

  output_surface_ = output_surface;
  surface_factory_ = base::MakeUnique<cc::SurfaceFactory>(
      GetEmbedderDeps()->GetSurfaceManager(), this);
}

void BlimpCompositor::SwapCompositorFrame(cc::CompositorFrame frame) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(surface_factory_);

  cc::RenderPass* root_pass =
      frame.delegated_frame_data->render_pass_list.back().get();
  gfx::Size surface_size = root_pass->output_rect.size();

  if (surface_id_.is_null() || current_surface_size_ != surface_size) {
    DestroyDelegatedContent();
    DCHECK(layer_->children().empty());

    surface_id_ = surface_id_allocator_->GenerateId();
    surface_factory_->Create(surface_id_);
    current_surface_size_ = surface_size;

    // manager must outlive compositors using it.
    cc::SurfaceManager* surface_manager =
        GetEmbedderDeps()->GetSurfaceManager();
    scoped_refptr<cc::SurfaceLayer> content_layer = cc::SurfaceLayer::Create(
        base::Bind(&SatisfyCallback, base::Unretained(surface_manager)),
        base::Bind(&RequireCallback, base::Unretained(surface_manager)));
    content_layer->SetSurfaceId(surface_id_, 1.f, surface_size);
    content_layer->SetBounds(current_surface_size_);
    content_layer->SetIsDrawable(true);
    content_layer->SetContentsOpaque(true);

    layer_->AddChild(content_layer);
  }

  surface_factory_->SubmitCompositorFrame(surface_id_, std::move(frame),
                                          base::Closure());
}

void BlimpCompositor::UnbindOutputSurface() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(surface_factory_);

  DestroyDelegatedContent();
  surface_factory_.reset();
  output_surface_ = nullptr;
}

void BlimpCompositor::ReturnResources(
    const cc::ReturnedResourceArray& resources) {
  DCHECK(surface_factory_);
  compositor_dependencies_->GetCompositorTaskRunner()->PostTask(
      FROM_HERE, base::Bind(&BlimpOutputSurface::ReclaimCompositorResources,
                            output_surface_, resources));
}

CompositorDependencies* BlimpCompositor::GetEmbedderDeps() {
  return compositor_dependencies_->GetEmbedderDependencies();
}

void BlimpCompositor::DestroyDelegatedContent() {
  if (surface_id_.is_null())
    return;

  // Remove any references for the surface layer that uses this |surface_id_|.
  layer_->RemoveAllChildren();
  surface_factory_->Destroy(surface_id_);
  surface_id_ = cc::SurfaceId();
}

void BlimpCompositor::CreateLayerTreeHost(
    const cc::proto::InitializeImpl& initialize_message) {
  DCHECK(!host_);
  VLOG(1) << "Creating LayerTreeHost for render widget: " << render_widget_id_;

  // Create the LayerTreeHost
  cc::LayerTreeHost::InitParams params;
  params.client = this;
  params.task_graph_runner = compositor_dependencies_->GetTaskGraphRunner();
  params.gpu_memory_buffer_manager =
      GetEmbedderDeps()->GetGpuMemoryBufferManager();
  params.main_task_runner = base::ThreadTaskRunnerHandle::Get();
  params.image_serialization_processor =
      compositor_dependencies_->GetImageSerializationProcessor();
  params.settings = GetEmbedderDeps()->GetLayerTreeSettings();
  params.animation_host = cc::AnimationHost::CreateMainInstance();

  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner =
      compositor_dependencies_->GetCompositorTaskRunner();

  host_ = cc::LayerTreeHost::CreateRemoteClient(
      this /* remote_proto_channel */, compositor_task_runner, &params);
  host_->SetVisible(host_should_be_visible_);

  DCHECK(!input_manager_);
  input_manager_ = BlimpInputManager::Create(
      this, base::ThreadTaskRunnerHandle::Get(), compositor_task_runner,
      host_->GetInputHandler());
}

void BlimpCompositor::DestroyLayerTreeHost() {
  DCHECK(host_);
  VLOG(1) << "Destroying LayerTreeHost for render widget: "
          << render_widget_id_;
  // Tear down the output surface connection with the old LayerTreeHost
  // instance.
  DestroyDelegatedContent();
  surface_factory_.reset();

  // Destroy the old LayerTreeHost state.
  host_.reset();

  // Destroy the old input manager state.
  // It is important to destroy the LayerTreeHost before destroying the input
  // manager as it has a reference to the cc::InputHandlerClient owned by the
  // BlimpInputManager.
  input_manager_.reset();

  // Cancel any outstanding OutputSurface requests.  That way if we get an async
  // callback related to the old request we know to drop it.
  output_surface_request_pending_ = false;

  // Make sure we don't have a receiver at this point.
  DCHECK(!remote_proto_channel_receiver_);
}

}  // namespace client
}  // namespace blimp
