// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/aura/surface_binding.h"

#include <map>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/threading/thread_local.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/output_surface.h"
#include "cc/output/output_surface_client.h"
#include "cc/output/software_output_device.h"
#include "cc/resources/shared_bitmap_manager.h"
#include "mojo/aura/window_tree_host_mojo.h"
#include "mojo/cc/context_provider_mojo.h"
#include "mojo/cc/output_surface_mojo.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/surfaces/surfaces_type_converters.h"
#include "mojo/public/cpp/application/connect.h"
#include "mojo/public/interfaces/application/shell.mojom.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/interfaces/gpu/gpu.mojom.h"
#include "mojo/services/public/interfaces/surfaces/surfaces.mojom.h"
#include "mojo/services/public/interfaces/surfaces/surfaces_service.mojom.h"

namespace mojo {
namespace {

// SurfaceclientImpl -----------------------------------------------------------

class SurfaceClientImpl : public SurfaceClient {
 public:
  SurfaceClientImpl() {}
  ~SurfaceClientImpl() override {}

  // SurfaceClient:
  void ReturnResources(Array<ReturnedResourcePtr> resources) override {
    // TODO (sky|jamesr): figure out right way to recycle resources.
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SurfaceClientImpl);
};

// OutputSurface ---------------------------------------------------------------

// OutputSurface implementation for a view. Pushes the surface id to View when
// appropriate.
class OutputSurfaceImpl : public cc::OutputSurface {
 public:
  OutputSurfaceImpl(View* view,
                    const scoped_refptr<cc::ContextProvider>& context_provider,
                    Surface* surface,
                    cc::SurfaceIdAllocator* id_allocator);
  ~OutputSurfaceImpl() override;

  // cc::OutputSurface:
  void SwapBuffers(cc::CompositorFrame* frame) override;

 private:
  View* view_;
  Surface* surface_;
  cc::SurfaceIdAllocator* id_allocator_;
  cc::SurfaceId surface_id_;
  gfx::Size surface_size_;

  DISALLOW_COPY_AND_ASSIGN(OutputSurfaceImpl);
};

OutputSurfaceImpl::OutputSurfaceImpl(
    View* view,
    const scoped_refptr<cc::ContextProvider>& context_provider,
    Surface* surface,
    cc::SurfaceIdAllocator* id_allocator)
    : cc::OutputSurface(context_provider),
      view_(view),
      surface_(surface),
      id_allocator_(id_allocator) {
  capabilities_.delegated_rendering = true;
  capabilities_.max_frames_pending = 1;
}

OutputSurfaceImpl::~OutputSurfaceImpl() {
}

void OutputSurfaceImpl::SwapBuffers(cc::CompositorFrame* frame) {
  gfx::Size frame_size =
      frame->delegated_frame_data->render_pass_list.back()->output_rect.size();
  if (frame_size != surface_size_) {
    if (!surface_id_.is_null())
      surface_->DestroySurface(SurfaceId::From(surface_id_));
    surface_id_ = id_allocator_->GenerateId();
    surface_->CreateSurface(SurfaceId::From(surface_id_),
                            Size::From(frame_size));
    view_->SetSurfaceId(SurfaceId::From(surface_id_));
    surface_size_ = frame_size;
  }

  surface_->SubmitFrame(SurfaceId::From(surface_id_), Frame::From(*frame));

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
    : public base::RefCounted<PerViewManagerState> {
 public:
  static PerViewManagerState* Get(Shell* shell, ViewManager* view_manager);

  scoped_ptr<cc::OutputSurface> CreateOutputSurface(View* view);

 private:
  typedef std::map<ViewManager*, PerViewManagerState*> ViewManagerToStateMap;

  friend class base::RefCounted<PerViewManagerState>;

  PerViewManagerState(Shell* shell, ViewManager* view_manager);
  ~PerViewManagerState();

  void Init();

  // Callback when a Surface has been created.
  void OnCreatedSurfaceConnection(SurfacePtr surface, uint32_t id_namespace);

  static base::LazyInstance<
      base::ThreadLocalPointer<ViewManagerToStateMap>>::Leaky view_states;

  Shell* shell_;
  ViewManager* view_manager_;

  // Set of state needed to create an OutputSurface.
  scoped_ptr<SurfaceClient> surface_client_;
  GpuPtr gpu_;
  SurfacePtr surface_;
  SurfacesServicePtr surfaces_service_;
  scoped_ptr<cc::SurfaceIdAllocator> surface_id_allocator_;

  DISALLOW_COPY_AND_ASSIGN(PerViewManagerState);
};

// static
base::LazyInstance<base::ThreadLocalPointer<
    SurfaceBinding::PerViewManagerState::ViewManagerToStateMap>>::Leaky
    SurfaceBinding::PerViewManagerState::view_states;

// static
SurfaceBinding::PerViewManagerState* SurfaceBinding::PerViewManagerState::Get(
    Shell* shell,
    ViewManager* view_manager) {
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
SurfaceBinding::PerViewManagerState::CreateOutputSurface(View* view) {
  // TODO(sky): figure out lifetime here. Do I need to worry about the return
  // value outliving this?
  CommandBufferPtr cb;
  gpu_->CreateOffscreenGLES2Context(GetProxy(&cb));
  scoped_refptr<cc::ContextProvider> context_provider(
      new ContextProviderMojo(cb.PassMessagePipe()));
  return scoped_ptr<cc::OutputSurface>(new OutputSurfaceImpl(
      view, context_provider, surface_.get(), surface_id_allocator_.get()));
}

SurfaceBinding::PerViewManagerState::PerViewManagerState(
    Shell* shell,
    ViewManager* view_manager)
    : shell_(shell), view_manager_(view_manager) {
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
  DCHECK(!surfaces_service_.get());

  ServiceProviderPtr surfaces_service_provider;
  shell_->ConnectToApplication("mojo:surfaces_service",
                               GetProxy(&surfaces_service_provider));
  ConnectToService(surfaces_service_provider.get(), &surfaces_service_);
  // base::Unretained is ok here as we block until the call is received.
  surfaces_service_->CreateSurfaceConnection(
      base::Bind(&PerViewManagerState::OnCreatedSurfaceConnection,
                 base::Unretained(this)));
  // Block until we get the surface. This is done to make it easy for client
  // code. OTOH blocking is ick and leads to all sorts of problems.
  // TODO(sky): ick! There needs to be a better way to deal with this.
  surfaces_service_.WaitForIncomingMethodCall();
  DCHECK(surface_.get());
  surface_client_.reset(new SurfaceClientImpl);
  surface_.set_client(surface_client_.get());

  ServiceProviderPtr gpu_service_provider;
  // TODO(jamesr): Should be mojo:gpu_service
  shell_->ConnectToApplication("mojo:native_viewport_service",
                               GetProxy(&gpu_service_provider));
  ConnectToService(gpu_service_provider.get(), &gpu_);
}

void SurfaceBinding::PerViewManagerState::OnCreatedSurfaceConnection(
    SurfacePtr surface,
    uint32_t id_namespace) {
  surface_id_allocator_.reset(new cc::SurfaceIdAllocator(id_namespace));
  surface_ = surface.Pass();
}

// SurfaceBinding --------------------------------------------------------------

SurfaceBinding::SurfaceBinding(Shell* shell, View* view)
    : view_(view),
      state_(PerViewManagerState::Get(shell, view->view_manager())) {
}

SurfaceBinding::~SurfaceBinding() {
}

scoped_ptr<cc::OutputSurface> SurfaceBinding::CreateOutputSurface() {
  return state_->CreateOutputSurface(view_);
}

}  // namespace mojo
