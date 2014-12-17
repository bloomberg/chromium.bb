// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"

#include "ui/gfx/image/image_skia.h"

namespace test {

TestAppListControllerDelegate::TestAppListControllerDelegate() {
}

TestAppListControllerDelegate::~TestAppListControllerDelegate() {
}

void TestAppListControllerDelegate::DismissView() {
}

gfx::NativeWindow TestAppListControllerDelegate::GetAppListWindow() {
  return NULL;
}

gfx::ImageSkia TestAppListControllerDelegate::GetWindowIcon() {
  return gfx::ImageSkia();
}

bool TestAppListControllerDelegate::IsAppPinned(
    const std::string& extension_id) {
  return false;
}

void TestAppListControllerDelegate::PinApp(const std::string& extension_id) {
}

void TestAppListControllerDelegate::UnpinApp(const std::string& extension_id) {
}

AppListControllerDelegate::Pinnable
TestAppListControllerDelegate::GetPinnable() {
  return NO_PIN;
}

bool TestAppListControllerDelegate::CanDoCreateShortcutsFlow() {
  return false;
}

void TestAppListControllerDelegate::DoCreateShortcutsFlow(
    Profile* profile,
    const std::string& extension_id) {
}

bool TestAppListControllerDelegate::CanDoShowAppInfoFlow() {
  return false;
}

void TestAppListControllerDelegate::DoShowAppInfoFlow(
    Profile* profile,
    const std::string& extension_id) {
}

void TestAppListControllerDelegate::CreateNewWindow(Profile* profile,
                                                    bool incognito) {
}

void TestAppListControllerDelegate::OpenURL(Profile* profile,
                                            const GURL& url,
                                            ui::PageTransition transition,
                                            WindowOpenDisposition deposition) {
  last_opened_url_ = url;
}

void TestAppListControllerDelegate::ActivateApp(
    Profile* profile,
    const extensions::Extension* extension,
    AppListSource source,
    int event_flags) {
}

void TestAppListControllerDelegate::LaunchApp(
    Profile* profile,
    const extensions::Extension* extension,
    AppListSource source,
    int event_flags) {
}

void TestAppListControllerDelegate::ShowForProfileByPath(
    const base::FilePath& profile_path) {
}

bool TestAppListControllerDelegate::ShouldShowUserIcon() {
  return false;
}

}  // namespace test
