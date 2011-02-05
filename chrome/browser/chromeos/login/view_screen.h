// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_VIEW_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_VIEW_SCREEN_H_
#pragma once

#include "base/message_loop.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/wizard_screen.h"
#include "ui/gfx/size.h"

template <class V>
class ViewScreen : public WizardScreen {
 public:
  // Create screen with default size.
  explicit ViewScreen(WizardScreenDelegate* delegate);

  // Create screen with the specified size.
  ViewScreen(WizardScreenDelegate* delegate, int width, int height);
  virtual ~ViewScreen();

  // Overridden from WizardScreen:
  virtual void Show();
  virtual void Hide();
  virtual gfx::Size GetScreenSize() const { return size_; }

  V* view() { return view_; }

 protected:
  // Creates view object and adds it to views hierarchy.
  virtual void CreateView();
  // Creates view object.
  virtual V* AllocateView() = 0;

  // Refresh screen state.
  virtual void Refresh() {}

 private:
  // For testing automation
  friend class AutomationProvider;

  V* view_;

  // Size of the screen.
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(ViewScreen);
};

template <class V>
class DefaultViewScreen : public ViewScreen<V> {
 public:
  explicit DefaultViewScreen(WizardScreenDelegate* delegate)
        : ViewScreen<V>(delegate) {}
  DefaultViewScreen(WizardScreenDelegate* delegate, int width, int height)
      : ViewScreen<V>(delegate, width, height) {}
  V* AllocateView() {
    return new V(ViewScreen<V>::delegate()->GetObserver(this));
  }
};

///////////////////////////////////////////////////////////////////////////////
// ViewScreen, public:
template <class V>
ViewScreen<V>::ViewScreen(WizardScreenDelegate* delegate)
    : WizardScreen(delegate),
      view_(NULL),
      size_(chromeos::login::kWizardScreenWidth,
            chromeos::login::kWizardScreenHeight) {
}

template <class V>
ViewScreen<V>::ViewScreen(WizardScreenDelegate* delegate, int width, int height)
    : WizardScreen(delegate),
      view_(NULL),
      size_(width, height) {
}

template <class V>
ViewScreen<V>::~ViewScreen() {
  // Delete the view now. So we do not worry the View outlives its
  // controller.
  if (view_) {
    delete view_;
    view_ = NULL;
  }
}

///////////////////////////////////////////////////////////////////////////////
// ViewScreen, WizardScreen implementation:
template <class V>
void ViewScreen<V>::Show() {
  if (!view_) {
    CreateView();
  }
  view_->SetVisible(true);
  // After screen is initialized and shown refresh its model.
  // Refresh() is called after SetVisible(true) because screen handler
  // could exit right away.
  Refresh();
}

template <class V>
void ViewScreen<V>::Hide() {
  if (view_) {
    delegate()->GetWizardView()->RemoveChildView(view_);
    // RemoveChildView doesn't delete the view and we also can't delete it here
    // becuase we are in message processing for the view.
    MessageLoop::current()->DeleteSoon(FROM_HERE, view_);
    view_ = NULL;
  }
}

///////////////////////////////////////////////////////////////////////////////
// ViewScreen, protected:
template <class V>
void ViewScreen<V>::CreateView() {
  view_ = AllocateView();
  delegate()->GetWizardView()->AddChildView(view_);
  view_->Init();
  view_->SetVisible(false);
}

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_VIEW_SCREEN_H_
