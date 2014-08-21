// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/public/app_content_control_delegate.h"

namespace athena {

class AppContentControlDelegateImpl : public AppContentControlDelegate {
 public:
  AppContentControlDelegateImpl() {}
  virtual ~AppContentControlDelegateImpl() {}

  virtual bool UnloadApplication(
      const std::string& app_id,
      content::BrowserContext* browser_context) OVERRIDE;
  virtual bool RestartApplication(
      const std::string& app_id,
      content::BrowserContext* browser_context) OVERRIDE;
  virtual std::string GetApplicationID(
      content::WebContents* web_contents) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppContentControlDelegateImpl);
};

bool AppContentControlDelegateImpl::UnloadApplication(
    const std::string& app_id,
    content::BrowserContext* browser_context) {
  // TODO(skuhne): Use the extension system to unload
  // (|ExtensionService::TerminateExtension|) once it becomes available in
  // Athena.
  return false;
}

bool AppContentControlDelegateImpl::RestartApplication(
    const std::string& app_id,
    content::BrowserContext* browser_context) {
  return false;
}

std::string AppContentControlDelegateImpl::GetApplicationID(
    content::WebContents* web_contents) {
  return std::string();
}

// static
AppContentControlDelegate*
AppContentControlDelegate::CreateAppContentControlDelegate() {
  return new AppContentControlDelegateImpl;
}

}  // namespace athena
