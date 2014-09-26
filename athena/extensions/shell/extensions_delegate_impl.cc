// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/extensions/public/extensions_delegate.h"

#include "athena/extensions/shell/athena_shell_app_window_client.h"
#include "base/macros.h"
#include "extensions/common/extension_set.h"
#include "extensions/shell/browser/shell_extension_system.h"

namespace athena {
namespace {

class ShellExtensionsDelegate : public ExtensionsDelegate {
 public:
  explicit ShellExtensionsDelegate(content::BrowserContext* context)
      : context_(context),
        extension_system_(static_cast<extensions::ShellExtensionSystem*>(
            extensions::ExtensionSystem::Get(context))) {
    extensions::AppWindowClient::Set(&app_window_client_);
  }

  virtual ~ShellExtensionsDelegate() { extensions::AppWindowClient::Set(NULL); }

 private:
  // ExtensionsDelegate:
  virtual content::BrowserContext* GetBrowserContext() const OVERRIDE {
    return context_;
  }
  virtual const extensions::ExtensionSet& GetInstalledExtensions() OVERRIDE {
    shell_extensions_.Clear();
    if (extension_system_->extension().get())
      shell_extensions_.Insert(extension_system_->extension());
    return shell_extensions_;
  }
  virtual bool LaunchApp(const std::string& app_id) OVERRIDE {
    extension_system_->LaunchApp();
    return true;
  }

  virtual bool UnloadApp(const std::string& app_id) OVERRIDE { return false; }

  content::BrowserContext* context_;
  extensions::ShellExtensionSystem* extension_system_;
  extensions::ExtensionSet shell_extensions_;

  AthenaShellAppWindowClient app_window_client_;

  DISALLOW_COPY_AND_ASSIGN(ShellExtensionsDelegate);
};

}  // namespace

// static
void ExtensionsDelegate::CreateExtensionsDelegateForShell(
    content::BrowserContext* context) {
  new ShellExtensionsDelegate(context);
}

}  // namespace athena
