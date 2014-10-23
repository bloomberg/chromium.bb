// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/extensions/chrome/app_list_controller_delegate_athena.h"

#include "athena/activity/public/activity_factory.h"
#include "athena/extensions/public/extensions_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/common/extension.h"
#include "ui/app_list/views/app_list_view.h"

namespace athena {

AppListControllerDelegateAthena::AppListControllerDelegateAthena() {
}

AppListControllerDelegateAthena::~AppListControllerDelegateAthena() {
}

void AppListControllerDelegateAthena::DismissView() {
}

gfx::NativeWindow AppListControllerDelegateAthena::GetAppListWindow() {
  NOTIMPLEMENTED();
  return nullptr;
}

gfx::Rect AppListControllerDelegateAthena::GetAppListBounds() {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

gfx::ImageSkia AppListControllerDelegateAthena::GetWindowIcon() {
  return gfx::ImageSkia();
}

bool AppListControllerDelegateAthena::IsAppPinned(
    const std::string& extension_id) {
  return false;
}

void AppListControllerDelegateAthena::PinApp(const std::string& extension_id) {
  NOTREACHED();
}

void AppListControllerDelegateAthena::UnpinApp(
    const std::string& extension_id) {
  NOTREACHED();
}

AppListControllerDelegate::Pinnable
AppListControllerDelegateAthena::GetPinnable() {
  return NO_PIN;
}

void AppListControllerDelegateAthena::OnShowChildDialog() {
  NOTIMPLEMENTED();
}

void AppListControllerDelegateAthena::OnCloseChildDialog() {
  NOTIMPLEMENTED();
}

bool AppListControllerDelegateAthena::CanDoCreateShortcutsFlow() {
  return false;
}

void AppListControllerDelegateAthena::DoCreateShortcutsFlow(
    Profile* profile,
    const std::string& extension_id) {
  NOTREACHED();
}

void AppListControllerDelegateAthena::CreateNewWindow(Profile* profile,
                                                      bool incognito) {
  // Nothing needs to be done.
}

void AppListControllerDelegateAthena::OpenURL(
    Profile* profile,
    const GURL& url,
    ui::PageTransition transition,
    WindowOpenDisposition disposition) {
  ActivityFactory::Get()->CreateWebActivity(profile, base::string16(), url);
}

void AppListControllerDelegateAthena::ActivateApp(
    Profile* profile,
    const extensions::Extension* extension,
    AppListSource source,
    int event_flags) {
  LaunchApp(profile, extension, source, event_flags);
}

void AppListControllerDelegateAthena::LaunchApp(
    Profile* profile,
    const extensions::Extension* extension,
    AppListSource source,
    int event_flags) {
  ExtensionsDelegate::Get(profile)->LaunchApp(extension->id());
}

void AppListControllerDelegateAthena::ShowForProfileByPath(
    const base::FilePath& profile_path) {
  // Ash doesn't have profile switching.
  NOTREACHED();
}

bool AppListControllerDelegateAthena::ShouldShowUserIcon() {
  return false;
}

}  // namespace athena
