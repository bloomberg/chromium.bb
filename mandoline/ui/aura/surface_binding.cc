// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/ui/aura/surface_binding.h"

#include <map>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/threading/thread_local.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/output_surface.h"
#include "cc/output/output_surface_client.h"
#include "cc/output/software_output_device.h"
#include "cc/resources/shared_bitmap_manager.h"
#include "components/view_manager/public/cpp/view.h"
#include "components/view_manager/public/cpp/view_tree_connection.h"
#include "components/view_manager/public/interfaces/gpu.mojom.h"
#include "components/view_manager/public/interfaces/surfaces.mojom.h"
#include "mandoline/ui/aura/window_tree_host_mojo.h"
#include "mojo/application/public/cpp/connect.h"
#include "mojo/application/public/interfaces/shell.mojom.h"
#include "mojo/cc/context_provider_mojo.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/surfaces/surfaces_type_converters.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace mandoline {
namespace {

// OutputSurface ---------------------------------------------------------------

// OutputSurface implementation for a view. Pushes the surface id to View when
// appropriate.
class OutputSurfaceImpl : public cc::OutputSurface {
 public:
  OutputSurfaceImpl(mojo::View* view,
                    const scoped_refptr<cc::ContextProvider>& context_provider,
                    mojo::Surface* surface,
                    uint32_t id_namespace,
                    uint32_t* next_local_id);
  ~OutputSurfaceImpl() override;

  // cc::OutputSurface:
  void SwapBuffers(cc::CompositorFrame* frame) override;

 private:
  mojo::View* view_;
  mojo::Surface* surface_;
  uint32_t id_namespace_;
  uint32_t* next_local_id_;  // Owned by PerConnectionState.
  uint32_t local_id_;
  gfx::Size surface_size_;

  DISALLOW_COPY_AND_ASSIGN(OutputSurfaceImpl);
};

OutputSurfaceImpl::OutputSurfaceImpl(
    mojo::View* view,
    const scoped_refptr<cc::ContextProvider>& context_provider,
    mojo::Surface* surface,
    uint32_t id_namespace,
    uint32_t* next_local_id)
    : cc::OutputSurface(context_provider),
      view_(view),
      surface_(surface),
      id_namespace_(id_namespace),
      next_local_id_(next_local_id),
      local_id_(0u) {
  capabilities_.delegated_rendering = true;
  capabilities_.max_frames_pending = 1;
}

OutputSurfaceImpl::~OutputSurfaceImpl() {
}

void OutputSurfaceImpl::SwapBuffers(cc::CompositorFrame* frame) {
  gfx::Size frame_size =
      frame->delegated_frame_data->render_pass_list.back()->output_rect.size();
  if (frame_size != surface_size_) {
    if (local_id_ != 0u)
      surface_->DestroySurface(local_id_);
    local_id_ = (*next_local_id_)++;
    surface_->CreateSurface(local_id_);
    auto qualified_id = mojo::SurfaceId::New();
    qualified_id->local = local_id_;
    qualified_id->id_namespace = id_namespace_;
    view_->SetSurfaceId(qualified_id.Pass());
    surface_size_ = frame_size;
  }

  surface_->SubmitCompositorFrame(
      local_id_, mojo::CompositorFrame::From(*frame), mojo::Closure());

  client_->DidSwapBuffers();
  client_->DidSwapBuffersComplete();
}

}  // namespace

// PerConnectionState ----------------------------------------------------------

// State needed per ViewManager. Provides the real implementation of
// CreateOutputSurface. SurfaceBinding obtains a pointer to the
// PerConnectionState appropriate for the ViewManager. PerConnectionState is
// stored in a thread local map. When no more refereces to a PerConnectionState
// remain the PerConnectionState is deleted and the underlying map cleaned up.
class SurfaceBinding::PerConnectionState
    : public base::RefCounted<PerConnectionState>,
      public mojo::ResourceReturner {
 public:
  static PerConnectionState* Get(mojo::Shell* shell,
                                 mojo::ViewTreeConnection* connection);

  scoped_ptr<cc::OutputSurface> CreateOutputSurface(mojo::View* view);

 private:
  typedef std::map<mojo::ViewTreeConnection*,
                   PerConnectionState*> ConnectionToStateMap;

  friend class base::RefCounted<PerConnectionState>;

  PerConnectionState(mojo::Shell* shell, mojo::ViewTreeConnection* connection);
  ~PerConnectionState() override;

  void Init();

  // mojo::ResourceReturner:
  void ReturnResources(
      mojo::Array<mojo::ReturnedResourcePtr> resources) override;

  void SetIdNamespace(uint32_t id_namespace);

  static base::LazyInstance<
      base::ThreadLocalPointer<ConnectionToStateMap>>::Leaky view_states;

  mojo::Shell* shell_;
  mojo::ViewTreeConnection* connection_;

  // Set of state needed to create an OutputSurface.
  mojo::GpuPtr gpu_;
  mojo::SurfacePtr surface_;
  mojo::Binding<mojo::ResourceReturner> returner_binding_;
  uint32_t id_namespace_;
  uint32_t next_local_id_;

  DISALLOW_COPY_AND_ASSIGN(PerConnectionState);
};

// static
base::LazyInstance<base::ThreadLocalPointer<
    SurfaceBinding::PerConnectionState::ConnectionToStateMap>>::Leaky
    SurfaceBinding::PerConnectionState::view_states;

// static
SurfaceBinding::PerConnectionState* SurfaceBinding::PerConnectionState::Get(
    mojo::Shell* shell,
    mojo::ViewTreeConnection* connection) {
  ConnectionToStateMap* view_map = view_states.Pointer()->Get();
  if (!view_map) {
    view_map = new ConnectionToStateMap;
    view_states.Pointer()->Set(view_map);
  }
  if (!(*view_map)[connection]) {
    (*view_map)[connection] = new PerConnectionState(shell, connection);
    (*view_map)[connection]->Init();
  }
  return (*view_map)[connection];
}

scoped_ptr<cc::OutputSurface>
SurfaceBinding::PerConnectionState::CreateOutputSurface(mojo::View* view) {
  // TODO(sky): figure out lifetime here. Do I need to worry about the return
  // value outliving this?
  mojo::CommandBufferPtr cb;
  gpu_->CreateOffscreenGLES2Context(GetProxy(&cb));
  scoped_refptr<cc::ContextProvider> context_provider(
      new mojo::ContextProviderMojo(cb.PassInterface().PassHandle()));
  return make_scoped_ptr(new OutputSurfaceImpl(
      view, context_provider, surface_.get(), id_namespace_, &next_local_id_));
}

SurfaceBinding::PerConnectionState::PerConnectionState(
    mojo::Shell* shell,
    mojo::ViewTreeConnection* connection)
    : shell_(shell),
      connection_(connection),
      returner_binding_(this),
      id_namespace_(0u),
      next_local_id_(0u) {
}

SurfaceBinding::PerConnectionState::~PerConnectionState() {
  ConnectionToStateMap* view_map = view_states.Pointer()->Get();
  DCHECK(view_map);
  DCHECK_EQ(this, (*view_map)[connection_]);
  view_map->erase(connection_);
  if (view_map->empty()) {
    delete view_map;
    view_states.Pointer()->Set(nullptr);
  }
}

void SurfaceBinding::PerConnectionState::Init() {
  DCHECK(!surface_.get());

  mojo::ServiceProviderPtr surfaces_service_provider;
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = mojo::String::From("mojo:view_manager");
  shell_->ConnectToApplication(request.Pass(),
                               GetProxy(&surfaces_service_provider),
                               nullptr,
                               nullptr);
  ConnectToService(surfaces_service_provider.get(), &surface_);
  surface_->GetIdNamespace(
      base::Bind(&SurfaceBinding::PerConnectionState::SetIdNamespace,
                 base::Unretained(this)));
  // Block until we receive our id namespace.
  surface_.WaitForIncomingResponse();
  DCHECK_NE(0u, id_namespace_);

  mojo::ResourceReturnerPtr returner_ptr;
  returner_binding_.Bind(GetProxy(&returner_ptr));
  surface_->SetResourceReturner(returner_ptr.Pass());

  mojo::ServiceProviderPtr gpu_service_provider;
  // TODO(jamesr): Should be mojo:gpu_service
  mojo::URLRequestPtr request2(mojo::URLRequest::New());
  request2->url = mojo::String::From("mojo:view_manager");
  shell_->ConnectToApplication(request2.Pass(),
                               GetProxy(&gpu_service_provider),
                               nullptr,
                               nullptr);
  ConnectToService(gpu_service_provider.get(), &gpu_);
}

void SurfaceBinding::PerConnectionState::SetIdNamespace(
    uint32_t id_namespace) {
  id_namespace_ = id_namespace;
}

void SurfaceBinding::PerConnectionState::ReturnResources(
    mojo::Array<mojo::ReturnedResourcePtr> resources) {
}

// SurfaceBinding --------------------------------------------------------------

SurfaceBinding::SurfaceBinding(mojo::Shell* shell, mojo::View* view)
    : view_(view),
      state_(PerConnectionState::Get(shell, view->connection())) {
}

SurfaceBinding::~SurfaceBinding() {
}

scoped_ptr<cc::OutputSurface> SurfaceBinding::CreateOutputSurface() {
  return state_->CreateOutputSurface(view_);
}

}  // namespace mandoline
