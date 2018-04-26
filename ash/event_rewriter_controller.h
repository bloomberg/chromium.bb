// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_EVENT_REWRITER_CONTROLLER_H_
#define ASH_EVENT_REWRITER_CONTROLLER_H_

#include <list>
#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/aura/env_observer.h"

namespace ui {
class EventRewriter;
class EventSource;
}  // namespace ui

namespace ash {

// Owns ui::EventRewriters and ensures that they are added to each root window
// EventSource, current and future, in the order that they are added to this.
// TODO(crbug.com/647781): Avoid exposing this outside of ash.
class ASH_EXPORT EventRewriterController : public aura::EnvObserver {
 public:
  EventRewriterController();
  ~EventRewriterController() override;

  // Takes ownership of an EventRewriter; can only be called before Init().
  void AddEventRewriter(std::unique_ptr<ui::EventRewriter> rewriter);

  // Add rewriters to any existing root windows; must be called once only
  // after ash::Shell has been initialized.
  void Init();

  // aura::EnvObserver overrides:
  void OnWindowInitialized(aura::Window* window) override {}
  void OnHostInitialized(aura::WindowTreeHost* host) override;

 private:
  void AddToEventSource(ui::EventSource* source);

  // The |EventRewriter|s managed by this controller.
  std::vector<std::unique_ptr<ui::EventRewriter>> rewriters_;

  // Whether the owned event rewriters have been added to existing
  // root windows; after this no more rewriters can be added.
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(EventRewriterController);
};

}  // namespace ash

#endif  // ASH_EVENT_REWRITER_CONTROLLER_H_
