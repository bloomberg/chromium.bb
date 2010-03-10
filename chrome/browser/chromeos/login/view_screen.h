// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_VIEW_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_VIEW_SCREEN_H_

#include "chrome/browser/chromeos/login/login_manager_view.h"
#include "chrome/browser/chromeos/login/network_selection_view.h"
#include "chrome/browser/chromeos/login/update_view.h"
#include "chrome/browser/chromeos/login/wizard_screen.h"

template <class V>
class ViewScreen : public WizardScreen {
 public:
  explicit ViewScreen(WizardScreenDelegate* delegate);
  virtual ~ViewScreen();

  // Overridden from WizardScreen:
  virtual void Show();
  virtual void Hide();

 protected:
  // Creates view object and adds it to views hierarchy.
  virtual void InitView();
  // Creates view object.
  virtual V* CreateView() = 0;

  V* view() { return view_; }

 private:
  // For testing automation
  friend class AutomationProvider;

  V* view_;

  DISALLOW_COPY_AND_ASSIGN(ViewScreen);
};

template <class V>
class DefaultViewScreen : public ViewScreen<V> {
 public:
  explicit DefaultViewScreen(WizardScreenDelegate* delegate)
      : ViewScreen<V>(delegate) {}
  V* CreateView() {
    return new V(ViewScreen<V>::delegate()->GetObserver(this));
  }
};

///////////////////////////////////////////////////////////////////////////////
// ViewScreen, public:
template <class V>
ViewScreen<V>::ViewScreen(WizardScreenDelegate* delegate)
    : WizardScreen(delegate),
      view_(NULL) {
}

template <class V>
ViewScreen<V>::~ViewScreen() {
}

///////////////////////////////////////////////////////////////////////////////
// ViewScreen, WizardScreen implementation:
template <class V>
void ViewScreen<V>::Show() {
  if (!view_) {
    InitView();
  }
  view_->SetVisible(true);
  // After view is initialized and shown refresh it's state.
  view_->Refresh();
}

template <class V>
void ViewScreen<V>::Hide() {
  if (view_)
    view_->SetVisible(false);
}

///////////////////////////////////////////////////////////////////////////////
// ViewScreen, protected:
template <class V>
void ViewScreen<V>::InitView() {
  view_ = CreateView();
  delegate()->GetWizardView()->AddChildView(view_);
  view_->Init();
  view_->SetVisible(false);
}

typedef DefaultViewScreen<chromeos::LoginManagerView> LoginScreen;
typedef DefaultViewScreen<chromeos::NetworkSelectionView> NetworkScreen;
typedef DefaultViewScreen<chromeos::UpdateView> UpdateScreen;

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_VIEW_SCREEN_H_
