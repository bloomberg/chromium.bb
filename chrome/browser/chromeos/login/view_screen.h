// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_VIEW_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_VIEW_SCREEN_H_

#include "chrome/browser/chromeos/login/account_creation_view.h"
#include "chrome/browser/chromeos/login/login_manager_view.h"
#include "chrome/browser/chromeos/login/network_selection_view.h"
#include "chrome/browser/chromeos/login/wizard_screen.h"

template <class V>
class ViewScreen : public WizardScreen {
 public:
  explicit ViewScreen(WizardScreenDelegate* delegate);
  virtual ~ViewScreen();

  // Overridden from WizardScreen:
  virtual void Show();
  virtual void Hide();

 private:
  void InitView();

  V* view_;

  DISALLOW_COPY_AND_ASSIGN(ViewScreen);
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
}

template <class V>
void ViewScreen<V>::Hide() {
  if (view_)
    view_->SetVisible(false);
}

///////////////////////////////////////////////////////////////////////////////
// ViewScreen, private:
template <class V>
void ViewScreen<V>::InitView() {
  view_ = new V(delegate()->GetObserver(this));
  delegate()->GetWizardView()->AddChildView(view_);
  view_->Init();
  view_->SetVisible(false);
}

typedef ViewScreen<LoginManagerView> LoginScreen;
typedef ViewScreen<AccountCreationView> AccountScreen;
typedef ViewScreen<NetworkSelectionView> NetworkScreen;

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_VIEW_SCREEN_H_

