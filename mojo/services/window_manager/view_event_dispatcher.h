// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WINDOW_MANAGER_VIEW_EVENT_DISPATCHER_H_
#define SERVICES_WINDOW_MANAGER_VIEW_EVENT_DISPATCHER_H_

#include "base/memory/scoped_ptr.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_target.h"

namespace window_manager {

class ViewTarget;

class ViewEventDispatcher : public ui::EventProcessor {
 public:
  ViewEventDispatcher();
  ~ViewEventDispatcher() override;

  void SetRootViewTarget(ViewTarget* root_view_target);

 private:
  // Overridden from ui::EventProcessor:
  ui::EventTarget* GetRootTarget() override;
  void OnEventProcessingStarted(ui::Event* event) override;

  // Overridden from ui::EventDispatcherDelegate.
  bool CanDispatchToTarget(ui::EventTarget* target) override;
  ui::EventDispatchDetails PreDispatchEvent(ui::EventTarget* target,
                                            ui::Event* event) override;
  ui::EventDispatchDetails PostDispatchEvent(
      ui::EventTarget* target, const ui::Event& event) override;

  // We keep a weak reference to ViewTarget*, which corresponds to the root of
  // the mojo::View tree.
  ViewTarget* root_view_target_;

  ViewTarget* event_dispatch_target_;
  ViewTarget* old_dispatch_target_;

  DISALLOW_COPY_AND_ASSIGN(ViewEventDispatcher);
};

}  // namespace window_manager

#endif  // SERVICES_WINDOW_MANAGER_VIEW_EVENT_DISPATCHER_H_
