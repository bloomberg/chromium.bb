// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EVENTS_EVENT_REWRITER_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_EVENTS_EVENT_REWRITER_CONTROLLER_H_

#include <list>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "ui/aura/env_observer.h"
#include "ui/events/event_rewriter.h"

namespace ui {
class EventSource;
}  // namespace ui

namespace chromeos {

// Owns |ui::EventRewriter|s and ensures that they are added to all root
// windows |EventSource|s, current and future, in the order that they are
// added to this.
class EventRewriterController : public aura::EnvObserver {
 public:
  EventRewriterController();
  virtual ~EventRewriterController();

  // Takes ownership of an EventRewriter; can only be called before Init().
  void AddEventRewriter(scoped_ptr<ui::EventRewriter> rewriter);

  // Add rewriters to any existing root windows; must be called once only
  // after ash::Shell has been initialized.
  void Init();

  // aura::EnvObserver overrides:
  virtual void OnWindowInitialized(aura::Window* window) OVERRIDE {}
  virtual void OnHostInitialized(aura::WindowTreeHost* host) OVERRIDE;

 private:
  typedef std::list<ui::EventSource*> EventSourceList;
  typedef ScopedVector<ui::EventRewriter> EventRewriters;

  void AddToEventSource(ui::EventSource* source);

  // The |EventRewriter|s managed by this controller.
  EventRewriters rewriters_;

  // Whether the owned event rewriters have been added to existing
  // root windows; after this no more rewriters can be added.
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(EventRewriterController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EVENTS_EVENT_REWRITER_CONTROLLER_H_
