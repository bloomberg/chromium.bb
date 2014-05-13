// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/events/event_rewriter_controller.h"

#include "ash/shell.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event_source.h"

namespace chromeos {

EventRewriterController::EventRewriterController() : initialized_(false) {
  // Add the controller as an observer for new root windows.
  aura::Env::GetInstance()->AddObserver(this);
}

EventRewriterController::~EventRewriterController() {
  aura::Env::GetInstance()->RemoveObserver(this);
  // Remove the rewriters from every root window EventSource and destroy them.
  for (EventRewriters::iterator rewriter_iter = rewriters_.begin();
       rewriter_iter != rewriters_.end();
       ++rewriter_iter) {
    aura::Window::Windows windows = ash::Shell::GetAllRootWindows();
    for (aura::Window::Windows::iterator window_iter = windows.begin();
         window_iter != windows.end();
         ++window_iter) {
      (*window_iter)->GetHost()->GetEventSource()->RemoveEventRewriter(
          *rewriter_iter);
    }
  }
  rewriters_.clear();
}

void EventRewriterController::AddEventRewriter(
    scoped_ptr<ui::EventRewriter> rewriter) {
  DCHECK(!initialized_);
  rewriters_.push_back(rewriter.release());
}

void EventRewriterController::Init() {
  DCHECK(!initialized_);
  initialized_ = true;
  // Add the rewriters to each existing root window EventSource.
  aura::Window::Windows windows = ash::Shell::GetAllRootWindows();
  for (aura::Window::Windows::iterator it = windows.begin();
       it != windows.end();
       ++it) {
    AddToEventSource((*it)->GetHost()->GetEventSource());
  }
}

void EventRewriterController::OnHostInitialized(aura::WindowTreeHost* host) {
  if (initialized_)
    AddToEventSource(host->GetEventSource());
}

void EventRewriterController::AddToEventSource(ui::EventSource* source) {
  DCHECK(source);
  for (EventRewriters::iterator it = rewriters_.begin(); it != rewriters_.end();
       ++it) {
    source->AddEventRewriter(*it);
  }
}

}  // namespace chromeos
