// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/ui/app_list/app_list_shower.h"
#include "content/public/browser/browser_context.h"

AppListShower::AppListShower(scoped_ptr<AppListFactory> factory,
                             scoped_ptr<KeepAliveService> keep_alive)
    : factory_(factory.Pass()),
      keep_alive_service_(keep_alive.Pass()),
      browser_context_(NULL),
      can_close_app_list_(true) {
}

AppListShower::~AppListShower() {
}

void AppListShower::ShowAndReacquireFocus(
    content::BrowserContext* requested_context) {
  ShowForBrowserContext(requested_context);
  app_list_->RegainNextLostFocus();
}

void AppListShower::ShowForBrowserContext(
    content::BrowserContext* requested_context) {
  // If the app list is already displaying |requested_context| just activate it
  // (in case we have lost focus).
  if (IsAppListVisible() && (requested_context == browser_context_)) {
    app_list_->Show();
    return;
  }

  if (!app_list_) {
    CreateViewForBrowserContext(requested_context);
  } else if (requested_context != browser_context_) {
    browser_context_ = requested_context;
    app_list_->SetBrowserContext(requested_context);
  }

  keep_alive_service_->EnsureKeepAlive();
  if (!IsAppListVisible())
    app_list_->MoveNearCursor();
  app_list_->Show();
}

gfx::NativeWindow AppListShower::GetWindow() {
  if (!IsAppListVisible())
    return NULL;
  return app_list_->GetWindow();
}

void AppListShower::CreateViewForBrowserContext(
    content::BrowserContext* requested_context) {
  // Aura has problems with layered windows and bubble delegates. The app
  // launcher has a trick where it only hides the window when it is dismissed,
  // reshowing it again later. This does not work with win aura for some
  // reason. This change temporarily makes it always get recreated, only on
  // win aura. See http://crbug.com/176186.
#if !defined(USE_AURA)
  if (requested_context == browser_context_)
    return;
#endif

  browser_context_ = requested_context;
  app_list_.reset(factory_->CreateAppList(
      browser_context_,
      base::Bind(&AppListShower::DismissAppList, base::Unretained(this))));
}

void AppListShower::DismissAppList() {
  if (app_list_ && can_close_app_list_) {
    app_list_->Hide();
    keep_alive_service_->FreeKeepAlive();
  }
}

void AppListShower::CloseAppList() {
  app_list_.reset();
  browser_context_ = NULL;

  // We may end up here as the result of the OS deleting the AppList's
  // widget (WidgetObserver::OnWidgetDestroyed). If this happens and there
  // are no browsers around then deleting the keep alive will result in
  // deleting the Widget again (by way of CloseAllSecondaryWidgets). When
  // the stack unravels we end up back in the Widget that was deleted and
  // crash. By delaying deletion of the keep alive we ensure the Widget has
  // correctly been destroyed before ending the keep alive so that
  // CloseAllSecondaryWidgets() won't attempt to delete the AppList's Widget
  // again.
  if (base::MessageLoop::current()) {  // NULL in tests.
    base::MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(&KeepAliveService::FreeKeepAlive,
                   base::Unretained(keep_alive_service_.get())));
    return;
  }
  keep_alive_service_->FreeKeepAlive();
}

bool AppListShower::IsAppListVisible() const {
  return app_list_ && app_list_->IsVisible();
}

void AppListShower::WarmupForProfile(content::BrowserContext* context) {
  DCHECK(!browser_context_);
  CreateViewForBrowserContext(context);
  app_list_->Prerender();
}

bool AppListShower::HasView() const {
  return !!app_list_;
}
