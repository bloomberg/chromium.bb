// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/extensions/public/extensions_delegate.h"
#include "base/macros.h"
#include "extensions/common/extension_set.h"
#include "extensions/shell/browser/shell_extension_system.h"

namespace athena {
namespace {

ExtensionsDelegate* instance = NULL;

class ShellExtensionsDelegate : public ExtensionsDelegate {
 public:
  explicit ShellExtensionsDelegate(content::BrowserContext* context)
      : context_(context),
        extension_system_(static_cast<extensions::ShellExtensionSystem*>(
            extensions::ExtensionSystem::Get(context))) {}

  virtual ~ShellExtensionsDelegate() {}

 private:
  // ExtensionsDelegate:
  virtual content::BrowserContext* GetBrowserContext() const OVERRIDE {
    return context_;
  }
  virtual const extensions::ExtensionSet& GetInstalledExtensions() OVERRIDE {
    shell_extensions_.Clear();
    if (extension_system_->extension())
      shell_extensions_.Insert(extension_system_->extension());
    return shell_extensions_;
  }
  virtual void LaunchApp(const std::string& app_id) OVERRIDE {
    extension_system_->LaunchApp();
  }

  content::BrowserContext* context_;
  extensions::ShellExtensionSystem* extension_system_;
  extensions::ExtensionSet shell_extensions_;

  DISALLOW_COPY_AND_ASSIGN(ShellExtensionsDelegate);
};

}  // namespace

ExtensionsDelegate::ExtensionsDelegate() {
  DCHECK(!instance);
  instance = this;
}

ExtensionsDelegate::~ExtensionsDelegate() {
  DCHECK(instance);
  instance = NULL;
}

// static
ExtensionsDelegate* ExtensionsDelegate::Get(content::BrowserContext* context) {
  DCHECK(instance);
  DCHECK_EQ(context, instance->GetBrowserContext());
  return instance;
}

// static
void ExtensionsDelegate::CreateExtensionsDelegateForShell(
    content::BrowserContext* context) {
  new ShellExtensionsDelegate(context);
}

// static
void ExtensionsDelegate::Shutdown() {
  DCHECK(instance);
  delete instance;
}

}  // namespace athena
