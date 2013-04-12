// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_surface_tracker.h"

#if defined(OS_ANDROID)
#include <android/native_window_jni.h>
#endif  // defined(OS_ANDROID)

#include "base/logging.h"

#if defined(TOOLKIT_GTK)
#include "base/bind.h"
#include "content/public/browser/browser_thread.h"
#include "ui/gfx/gtk_native_view_id_manager.h"
#endif  // defined(TOOLKIT_GTK)

namespace content {

namespace {
#if defined(TOOLKIT_GTK)

void ReleasePermanentXIDDispatcher(
    const gfx::PluginWindowHandle& surface) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  GtkNativeViewManager* manager = GtkNativeViewManager::GetInstance();
  manager->ReleasePermanentXID(surface);
}

// Implementation of SurfaceRef that allows GTK to ref and unref the
// surface with the GtkNativeViewManager.
class SurfaceRefPluginWindow : public GpuSurfaceTracker::SurfaceRef {
  public:
   SurfaceRefPluginWindow(const gfx::PluginWindowHandle& surface_ref);
  private:
   virtual ~SurfaceRefPluginWindow();
   gfx::PluginWindowHandle surface_;
};

SurfaceRefPluginWindow::SurfaceRefPluginWindow(
    const gfx::PluginWindowHandle& surface)
    : surface_(surface) {
  if (surface_ != gfx::kNullPluginWindow) {
    GtkNativeViewManager* manager = GtkNativeViewManager::GetInstance();
    if (!manager->AddRefPermanentXID(surface_)) {
      LOG(ERROR) << "Surface " << surface << " cannot be referenced.";
    }
  }
}

SurfaceRefPluginWindow::~SurfaceRefPluginWindow() {
  if (surface_ != gfx::kNullPluginWindow) {
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(&ReleasePermanentXIDDispatcher,
                                       surface_));
  }
}
#endif  // defined(TOOLKIT_GTK)
}  // anonymous

GpuSurfaceTracker::GpuSurfaceTracker()
    : next_surface_id_(1) {
  GpuSurfaceLookup::InitInstance(this);
}

GpuSurfaceTracker::~GpuSurfaceTracker() {
  GpuSurfaceLookup::InitInstance(NULL);
}

GpuSurfaceTracker* GpuSurfaceTracker::GetInstance() {
  return Singleton<GpuSurfaceTracker>::get();
}

int GpuSurfaceTracker::AddSurfaceForRenderer(int renderer_id,
                                             int render_widget_id) {
  base::AutoLock lock(lock_);
  int surface_id = next_surface_id_++;
  surface_map_[surface_id] =
      SurfaceInfo(renderer_id, render_widget_id, gfx::kNullAcceleratedWidget,
                  gfx::GLSurfaceHandle(), NULL);
  return surface_id;
}

int GpuSurfaceTracker::LookupSurfaceForRenderer(int renderer_id,
                                                int render_widget_id) {
  base::AutoLock lock(lock_);
  for (SurfaceMap::iterator it = surface_map_.begin(); it != surface_map_.end();
       ++it) {
    const SurfaceInfo& info = it->second;
    if (info.renderer_id == renderer_id &&
        info.render_widget_id == render_widget_id) {
      return it->first;
    }
  }
  return 0;
}

int GpuSurfaceTracker::AddSurfaceForNativeWidget(
    gfx::AcceleratedWidget widget) {
  base::AutoLock lock(lock_);
  int surface_id = next_surface_id_++;
  surface_map_[surface_id] =
      SurfaceInfo(0, 0, widget, gfx::GLSurfaceHandle(), NULL);
  return surface_id;
}

void GpuSurfaceTracker::RemoveSurface(int surface_id) {
  base::AutoLock lock(lock_);
  DCHECK(surface_map_.find(surface_id) != surface_map_.end());
  surface_map_.erase(surface_id);
}

bool GpuSurfaceTracker::GetRenderWidgetIDForSurface(int surface_id,
                                                    int* renderer_id,
                                                    int* render_widget_id) {
  base::AutoLock lock(lock_);
  SurfaceMap::iterator it = surface_map_.find(surface_id);
  if (it == surface_map_.end())
    return false;
  const SurfaceInfo& info = it->second;
  *renderer_id = info.renderer_id;
  *render_widget_id = info.render_widget_id;
  return true;
}

void GpuSurfaceTracker::SetSurfaceHandle(int surface_id,
                                         const gfx::GLSurfaceHandle& handle) {
  base::AutoLock lock(lock_);
  DCHECK(surface_map_.find(surface_id) != surface_map_.end());
  SurfaceInfo& info = surface_map_[surface_id];
  info.handle = handle;
#if defined(TOOLKIT_GTK)
  info.surface_ref = new SurfaceRefPluginWindow(handle.handle);
#endif  // defined(TOOLKIT_GTK)
}

gfx::GLSurfaceHandle GpuSurfaceTracker::GetSurfaceHandle(int surface_id) {
  base::AutoLock lock(lock_);
  SurfaceMap::iterator it = surface_map_.find(surface_id);
  if (it == surface_map_.end())
    return gfx::GLSurfaceHandle();
  return it->second.handle;
}

gfx::AcceleratedWidget GpuSurfaceTracker::AcquireNativeWidget(int surface_id) {
  base::AutoLock lock(lock_);
  SurfaceMap::iterator it = surface_map_.find(surface_id);
  if (it == surface_map_.end())
    return gfx::kNullAcceleratedWidget;

#if defined(OS_ANDROID)
  if (it->second.native_widget != gfx::kNullAcceleratedWidget)
    ANativeWindow_acquire(it->second.native_widget);
#endif  // defined(OS_ANDROID)

  return it->second.native_widget;
}

void GpuSurfaceTracker::SetNativeWidget(
    int surface_id, gfx::AcceleratedWidget widget,
    SurfaceRef* surface_ref) {
  base::AutoLock lock(lock_);
  SurfaceMap::iterator it = surface_map_.find(surface_id);
  DCHECK(it != surface_map_.end());
  SurfaceInfo& info = it->second;
  info.native_widget = widget;
  info.surface_ref = surface_ref;
}

std::size_t GpuSurfaceTracker::GetSurfaceCount() {
  base::AutoLock lock(lock_);
  return surface_map_.size();
}

GpuSurfaceTracker::SurfaceInfo::SurfaceInfo()
   : renderer_id(0),
     render_widget_id(0),
     native_widget(gfx::kNullAcceleratedWidget) { }

GpuSurfaceTracker::SurfaceInfo::SurfaceInfo(
    int renderer_id,
    int render_widget_id,
    const gfx::AcceleratedWidget& native_widget,
    const gfx::GLSurfaceHandle& handle,
    const scoped_refptr<SurfaceRef>& surface_ref)
    : renderer_id(renderer_id),
      render_widget_id(render_widget_id),
      native_widget(native_widget),
      handle(handle),
      surface_ref(surface_ref) { }

GpuSurfaceTracker::SurfaceInfo::~SurfaceInfo() { }


}  // namespace content
