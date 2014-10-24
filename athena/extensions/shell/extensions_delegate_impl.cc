// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/extensions/public/extensions_delegate.h"

#include "athena/extensions/shell/athena_shell_app_window_client.h"
#include "base/macros.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/install/extension_install_ui.h"
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

  ~ShellExtensionsDelegate() override {
    extensions::AppWindowClient::Set(nullptr);
  }

 private:
  // ExtensionsDelegate:
  content::BrowserContext* GetBrowserContext() const override {
    return context_;
  }

  const extensions::ExtensionSet& GetInstalledExtensions() override {
    return extensions::ExtensionRegistry::Get(context_)->enabled_extensions();
  }
  bool LaunchApp(const std::string& app_id) override {
    extension_system_->LaunchApp(app_id);
    return true;
  }

  bool UnloadApp(const std::string& app_id) override { return false; }

  scoped_ptr<extensions::ExtensionInstallUI> CreateExtensionInstallUI()
      override {
    return scoped_ptr<extensions::ExtensionInstallUI>();
  }

  content::BrowserContext* context_;
  extensions::ShellExtensionSystem* extension_system_;

  AthenaShellAppWindowClient app_window_client_;

  DISALLOW_COPY_AND_ASSIGN(ShellExtensionsDelegate);
};

}  // namespace

// static
void ExtensionsDelegate::CreateExtensionsDelegate(
    content::BrowserContext* context) {
  new ShellExtensionsDelegate(context);
}

}  // namespace athena
