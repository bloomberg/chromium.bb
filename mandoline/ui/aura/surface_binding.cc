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
#include "components/gpu/public/interfaces/gpu.mojom.h"
#include "components/surfaces/public/interfaces/surfaces.mojom.h"
#include "components/view_manager/public/cpp/view.h"
#include "components/view_manager/public/cpp/view_manager.h"
#include "mandoline/ui/aura/window_tree_host_mojo.h"
#include "mojo/application/public/cpp/connect.h"
#include "mojo/application/public/interfaces/shell.mojom.h"
#include "mojo/cc/context_provider_mojo.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/surfaces/surfaces_type_converters.h"

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
  uint32_t* next_local_id_;  // Owned by PerViewManagerState.
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

  surface_->SubmitFrame(local_id_, mojo::Frame::From(*frame), mojo::Closure());

  client_->DidSwapBuffers();
  client_->DidSwapBuffersComplete();
}

}  // namespace

// PerViewManagerState ---------------------------------------------------------

// State needed per ViewManager. Provides the real implementation of
// CreateOutputSurface. SurfaceBinding obtains a pointer to the
// PerViewManagerState appropriate for the ViewManager. PerViewManagerState is
// stored in a thread local map. When no more refereces to a PerViewManagerState
// remain the PerViewManagerState is deleted and the underlying map cleaned up.
class SurfaceBinding::PerViewManagerState
    : public base::RefCounted<PerViewManagerState>,
      public mojo::ResourceReturner {
 public:
  static PerViewManagerState* Get(mojo::Shell* shell,
                                  mojo::ViewManager* view_manager);

  scoped_ptr<cc::OutputSurface> CreateOutputSurface(mojo::View* view);

 private:
  typedef std::map<mojo::ViewManager*,
                   PerViewManagerState*> ViewManagerToStateMap;

  friend class base::RefCounted<PerViewManagerState>;

  PerViewManagerState(mojo::Shell* shell, mojo::ViewManager* view_manager);
  ~PerViewManagerState() override;

  void Init();

  // mojo::ResourceReturner:
  void ReturnResources(
      mojo::Array<mojo::ReturnedResourcePtr> resources) override;

  void SetIdNamespace(uint32_t id_namespace);

  static base::LazyInstance<
      base::ThreadLocalPointer<ViewManagerToStateMap>>::Leaky view_states;

  mojo::Shell* shell_;
  mojo::ViewManager* view_manager_;

  // Set of state needed to create an OutputSurface.
  mojo::GpuPtr gpu_;
  mojo::SurfacePtr surface_;
  mojo::Binding<mojo::ResourceReturner> returner_binding_;
  uint32_t id_namespace_;
  uint32_t next_local_id_;

  DISALLOW_COPY_AND_ASSIGN(PerViewManagerState);
};

// static
base::LazyInstance<base::ThreadLocalPointer<
    SurfaceBinding::PerViewManagerState::ViewManagerToStateMap>>::Leaky
    SurfaceBinding::PerViewManagerState::view_states;

// static
SurfaceBinding::PerViewManagerState* SurfaceBinding::PerViewManagerState::Get(
    mojo::Shell* shell,
    mojo::ViewManager* view_manager) {
  ViewManagerToStateMap* view_map = view_states.Pointer()->Get();
  if (!view_map) {
    view_map = new ViewManagerToStateMap;
    view_states.Pointer()->Set(view_map);
  }
  if (!(*view_map)[view_manager]) {
    (*view_map)[view_manager] = new PerViewManagerState(shell, view_manager);
    (*view_map)[view_manager]->Init();
  }
  return (*view_map)[view_manager];
}

scoped_ptr<cc::OutputSurface>
SurfaceBinding::PerViewManagerState::CreateOutputSurface(mojo::View* view) {
  // TODO(sky): figure out lifetime here. Do I need to worry about the return
  // value outliving this?
  mojo::CommandBufferPtr cb;
  gpu_->CreateOffscreenGLES2Context(GetProxy(&cb));
  scoped_refptr<cc::ContextProvider> context_provider(
      new mojo::ContextProviderMojo(cb.PassInterface().PassHandle()));
  return make_scoped_ptr(new OutputSurfaceImpl(
      view, context_provider, surface_.get(), id_namespace_, &next_local_id_));
}

SurfaceBinding::PerViewManagerState::PerViewManagerState(
    mojo::Shell* shell,
    mojo::ViewManager* view_manager)
    : shell_(shell),
      view_manager_(view_manager),
      returner_binding_(this),
      id_namespace_(0u),
      next_local_id_(0u) {
}

SurfaceBinding::PerViewManagerState::~PerViewManagerState() {
  ViewManagerToStateMap* view_map = view_states.Pointer()->Get();
  DCHECK(view_map);
  DCHECK_EQ(this, (*view_map)[view_manager_]);
  view_map->erase(view_manager_);
  if (view_map->empty()) {
    delete view_map;
    view_states.Pointer()->Set(nullptr);
  }
}

void SurfaceBinding::PerViewManagerState::Init() {
  DCHECK(!surface_.get());

  mojo::ServiceProviderPtr surfaces_service_provider;
  shell_->ConnectToApplication("mojo:surfaces_service",
                               GetProxy(&surfaces_service_provider),
                               nullptr);
  ConnectToService(surfaces_service_provider.get(), &surface_);
  surface_->GetIdNamespace(
      base::Bind(&SurfaceBinding::PerViewManagerState::SetIdNamespace,
                 base::Unretained(this)));
  // Block until we receive our id namespace.
  surface_.WaitForIncomingMethodCall();
  DCHECK_NE(0u, id_namespace_);

  mojo::ResourceReturnerPtr returner_ptr;
  returner_binding_.Bind(GetProxy(&returner_ptr));
  surface_->SetResourceReturner(returner_ptr.Pass());

  mojo::ServiceProviderPtr gpu_service_provider;
  // TODO(jamesr): Should be mojo:gpu_service
  shell_->ConnectToApplication("mojo:native_viewport_service",
                               GetProxy(&gpu_service_provider),
                               nullptr);
  ConnectToService(gpu_service_provider.get(), &gpu_);
}

void SurfaceBinding::PerViewManagerState::SetIdNamespace(
    uint32_t id_namespace) {
  id_namespace_ = id_namespace;
}

void SurfaceBinding::PerViewManagerState::ReturnResources(
    mojo::Array<mojo::ReturnedResourcePtr> resources) {
}

// SurfaceBinding --------------------------------------------------------------

SurfaceBinding::SurfaceBinding(mojo::Shell* shell, mojo::View* view)
    : view_(view),
      state_(PerViewManagerState::Get(shell, view->view_manager())) {
}

SurfaceBinding::~SurfaceBinding() {
}

scoped_ptr<cc::OutputSurface> SurfaceBinding::CreateOutputSurface() {
  return state_->CreateOutputSurface(view_);
}

}  // namespace mandoline
