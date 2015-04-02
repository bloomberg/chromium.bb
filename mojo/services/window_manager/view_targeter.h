// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WINDOW_MANAGER_VIEW_TARGETER_H_
#define SERVICES_WINDOW_MANAGER_VIEW_TARGETER_H_

#include "ui/events/event_targeter.h"

namespace window_manager {

class ViewTarget;

class ViewTargeter : public ui::EventTargeter {
 public:
  ViewTargeter();
  ~ViewTargeter() override;

 protected:
  // ui::EventTargeter:
  ui::EventTarget* FindTargetForEvent(ui::EventTarget* root,
                                      ui::Event* event) override;
  ui::EventTarget* FindTargetForLocatedEvent(ui::EventTarget* root,
                                             ui::LocatedEvent* event) override;
  bool SubtreeCanAcceptEvent(ui::EventTarget* target,
                             const ui::LocatedEvent& event) const override;
  bool EventLocationInsideBounds(ui::EventTarget* target,
                                 const ui::LocatedEvent& event) const override;

 private:
  // Targets either the root View or the currently focused view.
  ViewTarget* FindTargetForKeyEvent(ViewTarget* view, const ui::KeyEvent& key);

  // Deals with cases where the |root_view| needs to change how things are
  // dispatched. (For example, in the case of capture.)
  ViewTarget* FindTargetInRootView(ViewTarget* root_view,
                                   const ui::LocatedEvent& event);

  DISALLOW_COPY_AND_ASSIGN(ViewTargeter);
};

}  // namespace window_manager

#endif  // SERVICES_WINDOW_MANAGER_VIEW_TARGETER_H_
