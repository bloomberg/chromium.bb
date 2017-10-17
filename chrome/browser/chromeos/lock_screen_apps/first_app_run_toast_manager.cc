// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/lock_screen_apps/first_app_run_toast_manager.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/lock_screen_apps/toast_dialog_view.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/common/extension.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

namespace lock_screen_apps {

FirstAppRunToastManager::FirstAppRunToastManager(Profile* profile)
    : profile_(profile),
      toast_widget_observer_(this),
      app_window_observer_(this),
      weak_ptr_factory_(this) {}

FirstAppRunToastManager::~FirstAppRunToastManager() {
  Reset();
}

void FirstAppRunToastManager::RunForAppWindow(
    extensions::AppWindow* app_window) {
  if (app_window_)
    return;

  DCHECK(app_window->GetNativeWindow());

  const extensions::Extension* app = app_window->GetExtension();
  const base::DictionaryValue* toast_shown =
      profile_->GetPrefs()->GetDictionary(
          prefs::kNoteTakingAppsLockScreenToastShown);
  bool already_shown_for_app = false;
  if (toast_shown->GetBoolean(app->id(), &already_shown_for_app) &&
      already_shown_for_app) {
    return;
  }

  app_window_ = app_window;

  if (app_window_->GetNativeWindow()->IsVisible())
    CreateAndShowToastDialog();
  else
    app_window_observer_.Add(app_window->GetNativeWindow());
}

void FirstAppRunToastManager::Reset() {
  app_window_observer_.RemoveAll();
  toast_widget_observer_.RemoveAll();

  app_window_ = nullptr;

  weak_ptr_factory_.InvalidateWeakPtrs();

  if (toast_widget_ && !toast_widget_->IsClosed())
    toast_widget_->Close();
  toast_widget_ = nullptr;
}

void FirstAppRunToastManager::OnWidgetDestroyed(views::Widget* widget) {
  Reset();
}

void FirstAppRunToastManager::OnWindowVisibilityChanged(aura::Window* window,
                                                        bool visible) {
  // NOTE: Use |window->IsVisible()|, rather than |visible| because |IsVisible|
  // takes into account whether the window is actually drawn (not just whether
  // window show has been called).
  if (!window->IsVisible())
    return;

  app_window_observer_.RemoveAll();

  CreateAndShowToastDialog();
}

void FirstAppRunToastManager::CreateAndShowToastDialog() {
  auto* toast_dialog = new ToastDialogView(
      base::UTF8ToUTF16(app_window_->GetExtension()->name()),
      base::Bind(&FirstAppRunToastManager::ToastDialogDismissed,
                 weak_ptr_factory_.GetWeakPtr()));
  toast_dialog->Show();
  toast_widget_ = toast_dialog->GetWidget();
  toast_widget_observer_.Add(toast_widget_);
}

void FirstAppRunToastManager::ToastDialogDismissed() {
  {
    const extensions::Extension* app = app_window_->GetExtension();
    DictionaryPrefUpdate dict_update(
        profile_->GetPrefs(), prefs::kNoteTakingAppsLockScreenToastShown);
    dict_update->SetBoolean(app->id(), true);
  }
  Reset();
}

}  // namespace lock_screen_apps
