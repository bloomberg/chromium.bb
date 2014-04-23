// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_VIEW_H_
#define MOJO_SERVICES_VIEW_MANAGER_VIEW_H_

#include "base/logging.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"
#include "mojo/services/view_manager/view_manager_export.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace mojo {
namespace services {
namespace view_manager {

class RootViewManager;
class ViewDelegate;
class ViewId;

class MOJO_VIEW_MANAGER_EXPORT View : public aura::WindowObserver {
 public:
  View(ViewDelegate* delegate, const int32_t id);
  ~View();

  int32 id() const { return id_; }

  void Add(View* child);
  void Remove(View* child);

  View* GetParent();

 private:
  ViewId GetViewId() const;

  // WindowObserver overrides:
  virtual void OnWindowHierarchyChanged(
      const aura::WindowObserver::HierarchyChangeParams& params) OVERRIDE;

  ViewDelegate* delegate_;
  const int32_t id_;
  aura::Window window_;

  DISALLOW_COPY_AND_ASSIGN(View);
};

}  // namespace view_manager
}  // namespace services
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_VIEW_H_
