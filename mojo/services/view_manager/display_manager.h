// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_DISPLAY_MANAGER_H_
#define MOJO_SERVICES_VIEW_MANAGER_DISPLAY_MANAGER_H_

#include <map>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "cc/surfaces/surface_id.h"
#include "mojo/public/cpp/bindings/callback.h"
#include "mojo/services/public/interfaces/native_viewport/native_viewport.mojom.h"
#include "mojo/services/public/interfaces/surfaces/surfaces.mojom.h"
#include "mojo/services/public/interfaces/surfaces/surfaces_service.mojom.h"
#include "mojo/services/view_manager/view_manager_export.h"
#include "ui/gfx/rect.h"

namespace cc {
class SurfaceIdAllocator;
}

namespace mojo {

class ApplicationConnection;

namespace service {

class ConnectionManager;
class ServerView;

// DisplayManager binds the root node to an actual display.
class MOJO_VIEW_MANAGER_EXPORT DisplayManager
    : NON_EXPORTED_BASE(public NativeViewportClient),
      NON_EXPORTED_BASE(public SurfaceClient) {
 public:
  DisplayManager(ApplicationConnection* app_connection,
                 ConnectionManager* connection_manager,
                 const Callback<void()>& native_viewport_closed_callback);
  virtual ~DisplayManager();

  // Schedules a paint for the specified region of the specified view.
  void SchedulePaint(const ServerView* view, const gfx::Rect& bounds);

  // See description above field for details.
  bool in_setup() const { return in_setup_; }

 private:
  void OnSurfaceConnectionCreated(SurfacePtr surface, uint32_t id_namespace);
  void Draw();

  // NativeViewportClient implementation.
  virtual void OnCreated(uint64_t native_viewport_id) OVERRIDE;
  virtual void OnDestroyed() OVERRIDE;
  virtual void OnBoundsChanged(SizePtr bounds) OVERRIDE;
  virtual void OnEvent(EventPtr event,
                       const mojo::Callback<void()>& callback) OVERRIDE;

  // SurfaceClient implementation.
  virtual void ReturnResources(Array<ReturnedResourcePtr> resources) OVERRIDE;

  ConnectionManager* connection_manager_;

  // Returns true if adding the root view's window to |window_tree_host_|.
  bool in_setup_;

  gfx::Size bounds_;
  gfx::Rect dirty_rect_;
  base::Timer draw_timer_;

  SurfacesServicePtr surfaces_service_;
  SurfacePtr surface_;
  scoped_ptr<cc::SurfaceIdAllocator> surface_id_allocator_;
  cc::SurfaceId surface_id_;
  NativeViewportPtr native_viewport_;
  Callback<void()> native_viewport_closed_callback_;
  base::WeakPtrFactory<DisplayManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DisplayManager);
};

}  // namespace service
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_DISPLAY_MANAGER_H_
