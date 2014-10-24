// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/extensions/public/extension_app_model_builder.h"

#include "athena/activity/public/activity_factory.h"
#include "athena/activity/public/activity_manager.h"
#include "athena/extensions/public/extensions_delegate.h"
#include "extensions/browser/extension_icon_image.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_model.h"
#include "ui/base/resource/resource_bundle.h"

namespace athena {

namespace {

// Copied from chrome/common/extensions/extension_constants.h
// TODO(mukai): move constants to src/extensions
const char kChromeAppId[] = "mgndgikekgjfcpckkfioiadnlibdjbkf";

class AppItem : public app_list::AppListItem {
 public:
  AppItem(scoped_refptr<const extensions::Extension> extension,
          content::BrowserContext* browser_context)
      : app_list::AppListItem(extension->id()),
        browser_context_(browser_context) {
    Reload(extension);
  }

  void Reload(scoped_refptr<const extensions::Extension> extension) {
    extension_ = extension;
    icon_image_.reset(new extensions::IconImage(
        browser_context_,
        extension.get(),
        extensions::IconsInfo::GetIcons(extension.get()),
        extension_misc::EXTENSION_ICON_MEDIUM,
        *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_APP_DEFAULT_ICON),
        nullptr));
    icon_image_->image_skia().EnsureRepsForSupportedScales();
    SetIcon(icon_image_->image_skia(), false);
    SetName(extension->name());
  }

 private:
  // Overridden from app_list::AppListItem:
  void Activate(int event_flags) override {
    ExtensionsDelegate::Get(browser_context_)->LaunchApp(extension_->id());
  }

  scoped_refptr<const extensions::Extension> extension_;
  content::BrowserContext* browser_context_;
  scoped_ptr<extensions::IconImage> icon_image_;

  DISALLOW_COPY_AND_ASSIGN(AppItem);
};

}  // namespace

ExtensionAppModelBuilder::ExtensionAppModelBuilder(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context), model_(nullptr) {
  extensions::ExtensionRegistryFactory::GetForBrowserContext(browser_context_)
      ->AddObserver(this);
}

ExtensionAppModelBuilder::~ExtensionAppModelBuilder() {
  extensions::ExtensionRegistryFactory::GetForBrowserContext(browser_context_)
      ->RemoveObserver(this);
}

void ExtensionAppModelBuilder::RegisterAppListModel(
    app_list::AppListModel* model) {
  DCHECK(!model_);
  model_ = model;

  ExtensionsDelegate* bridge = ExtensionsDelegate::Get(browser_context_);
  const extensions::ExtensionSet& extensions = bridge->GetInstalledExtensions();
  for (extensions::ExtensionSet::const_iterator iter = extensions.begin();
       iter != extensions.end();
       ++iter) {
    AddItem(*iter);
  }
}

void ExtensionAppModelBuilder::AddItem(
    scoped_refptr<const extensions::Extension> extension) {
  // Chrome icon is currently disabled for homecard since it's not meaningful.
  // http://crbug.com/421677
  // TODO(mukai): use chrome/browser/extension_ui_util.
  if (extension->ShouldDisplayInAppLauncher() &&
      extension->id() != kChromeAppId) {
    model_->AddItem(make_scoped_ptr(new AppItem(extension, browser_context_)));
  }
}

void ExtensionAppModelBuilder::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    bool is_update) {
  app_list::AppListItem* item = model_->FindItem(extension->id());
  if (item)
    static_cast<AppItem*>(item)->Reload(make_scoped_refptr(extension));
  else
    AddItem(make_scoped_refptr(extension));
}

void ExtensionAppModelBuilder::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  model_->DeleteItem(extension->id());
}

}  // namespace athena
