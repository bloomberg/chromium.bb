// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_EXTENSIONS_PUBLIC_EXTENSION_APP_MODEL_BUILDER_H_
#define ATHENA_EXTENSIONS_PUBLIC_EXTENSION_APP_MODEL_BUILDER_H_

#include "athena/home/public/app_model_builder.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "extensions/browser/extension_registry_observer.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;
}

namespace athena {

class ATHENA_EXPORT ExtensionAppModelBuilder
    : public AppModelBuilder,
      public extensions::ExtensionRegistryObserver {
 public:
  explicit ExtensionAppModelBuilder(content::BrowserContext* browser_context);
  ~ExtensionAppModelBuilder() override;

  void RegisterAppListModel(app_list::AppListModel* model) override;

 private:
  void AddItem(scoped_refptr<const extensions::Extension> extension);

  // extensions::ExtensionRegistryObserver:
  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const extensions::Extension* extension,
                            bool is_update) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;

  content::BrowserContext* browser_context_;

  // Unowned pointer to the app list model.
  app_list::AppListModel* model_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionAppModelBuilder);
};

}  // namespace athena

#endif  // ATHENA_EXTENSIONS_PUBLIC_EXTENSION_APP_MODEL_BUILDER_H_
