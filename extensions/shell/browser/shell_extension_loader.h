// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_EXTENSION_LOADER_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_EXTENSION_LOADER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/common/extension_id.h"

namespace base {
class FilePath;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

class Extension;

// Handles extension loading using ExtensionRegistrar.
class ShellExtensionLoader : public ExtensionRegistrar::Delegate {
 public:
  explicit ShellExtensionLoader(content::BrowserContext* browser_context);
  ~ShellExtensionLoader() override;

  // Loads an unpacked extension from a directory synchronously. Returns the
  // extension on success, or nullptr otherwise.
  const Extension* LoadExtension(const base::FilePath& extension_dir);

 private:
  // ExtensionRegistrar::Delegate:
  void PreAddExtension(const Extension* extension,
                       const Extension* old_extension) override;
  void PostActivateExtension(scoped_refptr<const Extension> extension) override;
  void PostDeactivateExtension(
      scoped_refptr<const Extension> extension) override;
  void LoadExtensionForReload(
      const ExtensionId& extension_id,
      const base::FilePath& path,
      ExtensionRegistrar::LoadErrorBehavior load_error_behavior) override;
  bool CanEnableExtension(const Extension* extension) override;
  bool CanDisableExtension(const Extension* extension) override;
  bool ShouldBlockExtension(const Extension* extension) override;

  content::BrowserContext* browser_context_;  // Not owned.

  // Registers and unregisters extensions.
  ExtensionRegistrar extension_registrar_;

  DISALLOW_COPY_AND_ASSIGN(ShellExtensionLoader);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_EXTENSION_LOADER_H_
