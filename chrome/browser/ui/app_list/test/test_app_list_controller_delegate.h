// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_TEST_TEST_APP_LIST_CONTROLLER_DELEGATE_H_
#define CHROME_BROWSER_UI_APP_LIST_TEST_TEST_APP_LIST_CONTROLLER_DELEGATE_H_

#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"

namespace test {

class TestAppListControllerDelegate : public AppListControllerDelegate {
 public:
  TestAppListControllerDelegate();
  virtual ~TestAppListControllerDelegate();

  virtual void DismissView() override;
  virtual gfx::NativeWindow GetAppListWindow() override;
  virtual gfx::ImageSkia GetWindowIcon() override;
  virtual bool IsAppPinned(const std::string& extension_id) override;
  virtual void PinApp(const std::string& extension_id) override;
  virtual void UnpinApp(const std::string& extension_id) override;
  virtual Pinnable GetPinnable() override;
  virtual bool CanDoCreateShortcutsFlow() override;
  virtual void DoCreateShortcutsFlow(Profile* profile,
                                     const std::string& extension_id) override;
  virtual bool CanDoShowAppInfoFlow() override;
  virtual void DoShowAppInfoFlow(Profile* profile,
                                 const std::string& extension_id) override;
  virtual void CreateNewWindow(Profile* profile, bool incognito) override;
  virtual void OpenURL(Profile* profile,
                       const GURL& url,
                       ui::PageTransition transition,
                       WindowOpenDisposition deposition) override;
  virtual void ActivateApp(Profile* profile,
                           const extensions::Extension* extension,
                           AppListSource source,
                           int event_flags) override;
  virtual void LaunchApp(Profile* profile,
                         const extensions::Extension* extension,
                         AppListSource source,
                         int event_flags) override;
  virtual void ShowForProfileByPath(
      const base::FilePath& profile_path) override;
  virtual bool ShouldShowUserIcon() override;
};

}  // namespace test

#endif  // CHROME_BROWSER_UI_APP_LIST_TEST_TEST_APP_LIST_CONTROLLER_DELEGATE_H_
