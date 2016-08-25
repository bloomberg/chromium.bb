// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/feature/compositor/blimp_compositor.h"

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "blimp/client/core/compositor/delegated_output_surface.h"
#include "blimp/client/feature/compositor/blimp_context_provider.h"
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

BlimpCompositor::BlimpCompositor(int render_widget_id,
                                 cc::SurfaceManager* surface_manager,
                                 uint32_t surface_client_id,
                                 BlimpCompositorClient* client)
    : render_widget_id_(render_widget_id),
      client_(client),
      host_should_be_visible_(false),
      output_surface_(nullptr),
      surface_manager_(surface_manager),
      surface_id_allocator_(
          base::MakeUnique<cc::SurfaceIdAllocator>(surface_client_id)),
      layer_(cc::Layer::Create()),
      remote_proto_channel_receiver_(nullptr),
      weak_factory_(this) {
  DCHECK(thread_checker_.CalledOnValidThread());
  surface_manager_->RegisterSurfaceClientId(surface_id_allocator_->client_id());
}

BlimpCompositor::~BlimpCompositor() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (host_)
    DestroyLayerTreeHost();
  surface_manager_->InvalidateSurfaceClientId(
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
  DCHECK(!surface_factory_)
      << "Any connection to the old output surface should have been destroyed";

  scoped_refptr<BlimpContextProvider> compositor_context_provider =
      BlimpContextProvider::Create(gfx::kNullAcceleratedWidget,
                                   client_->GetGpuMemoryBufferManager());

  // TODO(khushalsagar): Make a worker context and bind it to the current
  // thread:
  // Worker context is bound to the main thread in RenderThreadImpl. One day
  // that will change and then this will have to be removed.
  // worker_context_provider->BindToCurrentThread();

  std::unique_ptr<DelegatedOutputSurface> delegated_output_surface =
      base::MakeUnique<DelegatedOutputSurface>(
          std::move(compositor_context_provider), nullptr,
          base::ThreadTaskRunnerHandle::Get(), weak_factory_.GetWeakPtr());

  host_->SetOutputSurface(std::move(delegated_output_surface));
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
  const cc::proto::CompositorMessageToImpl& to_impl_proto =
      message->to_impl();

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

void BlimpCompositor::SendWebGestureEvent(
    const blink::WebGestureEvent& gesture_event) {
  client_->SendWebGestureEvent(render_widget_id_, gesture_event);
}

void BlimpCompositor::BindToOutputSurface(
    base::WeakPtr<BlimpOutputSurface> output_surface) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!output_surface_);
  DCHECK(!surface_factory_);

  output_surface_ = output_surface;
  surface_factory_ =
      base::MakeUnique<cc::SurfaceFactory>(surface_manager_, this);
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

    // Manager must outlive compositors using it.
    scoped_refptr<cc::SurfaceLayer> content_layer = cc::SurfaceLayer::Create(
        base::Bind(&SatisfyCallback, base::Unretained(surface_manager_)),
        base::Bind(&RequireCallback, base::Unretained(surface_manager_)));
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
  compositor_task_runner_->PostTask(
      FROM_HERE, base::Bind(&BlimpOutputSurface::ReclaimCompositorResources,
                            output_surface_, resources));
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
  params.task_graph_runner = client_->GetTaskGraphRunner();
  params.gpu_memory_buffer_manager = client_->GetGpuMemoryBufferManager();
  params.main_task_runner = base::ThreadTaskRunnerHandle::Get();
  params.image_serialization_processor =
      client_->GetImageSerializationProcessor();
  params.settings = client_->GetLayerTreeSettings();
  params.animation_host = cc::AnimationHost::CreateMainInstance();

  compositor_task_runner_ = client_->GetCompositorTaskRunner();

  host_ = cc::LayerTreeHost::CreateRemoteClient(
      this /* remote_proto_channel */, compositor_task_runner_, &params);
  host_->SetVisible(host_should_be_visible_);

  DCHECK(!input_manager_);
  input_manager_ = BlimpInputManager::Create(
      this, base::ThreadTaskRunnerHandle::Get(), compositor_task_runner_,
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

  // Make sure we don't have a receiver at this point.
  DCHECK(!remote_proto_channel_receiver_);
}

}  // namespace client
}  // namespace blimp
