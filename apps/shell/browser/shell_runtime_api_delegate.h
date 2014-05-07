// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_SHELL_BROWSER_SHELL_RUNTIME_API_DELEGATE_H_
#define APPS_SHELL_BROWSER_SHELL_RUNTIME_API_DELEGATE_H_

#include "base/macros.h"
#include "extensions/browser/api/runtime/runtime_api_delegate.h"

namespace apps {

class ShellRuntimeAPIDelegate : public extensions::RuntimeAPIDelegate {
 public:
  ShellRuntimeAPIDelegate();
  virtual ~ShellRuntimeAPIDelegate();

  // extensions::RuntimeAPIDelegate implementation.
  virtual void AddUpdateObserver(extensions::UpdateObserver* observer) OVERRIDE;
  virtual void RemoveUpdateObserver(
      extensions::UpdateObserver* observer) OVERRIDE;
  virtual base::Version GetPreviousExtensionVersion(
      const extensions::Extension* extension) OVERRIDE;
  virtual void ReloadExtension(const std::string& extension_id) OVERRIDE;
  virtual bool CheckForUpdates(const std::string& extension_id,
                               const UpdateCheckCallback& callback) OVERRIDE;
  virtual void OpenURL(const GURL& uninstall_url) OVERRIDE;
  virtual bool GetPlatformInfo(
      extensions::core_api::runtime::PlatformInfo* info) OVERRIDE;
  virtual bool RestartDevice(std::string* error_message) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellRuntimeAPIDelegate);
};

}  // namespace apps

#endif  // APPS_SHELL_BROWSER_SHELL_RUNTIME_API_DELEGATE_H_
