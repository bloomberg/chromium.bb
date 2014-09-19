// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/extensions/public/extension_app_model_builder.h"

#include "athena/activity/public/activity_factory.h"
#include "athena/activity/public/activity_manager.h"
#include "athena/extensions/public/extensions_delegate.h"
#include "extensions/browser/extension_icon_image.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_model.h"
#include "ui/base/resource/resource_bundle.h"

namespace athena {

namespace {

class AppItem : public app_list::AppListItem {
 public:
  AppItem(scoped_refptr<const extensions::Extension> extension,
          content::BrowserContext* browser_context)
      : app_list::AppListItem(extension->id()),
        extension_(extension),
        browser_context_(browser_context),
        icon_image_(browser_context_,
                    extension.get(),
                    extensions::IconsInfo::GetIcons(extension.get()),
                    extension_misc::EXTENSION_ICON_MEDIUM,
                    *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                        IDR_APP_DEFAULT_ICON),
                    NULL) {
    icon_image_.image_skia().EnsureRepsForSupportedScales();
    SetIcon(icon_image_.image_skia(), false);
    SetName(extension->name());
  }

 private:
  // Overridden from app_list::AppListItem:
  virtual void Activate(int event_flags) OVERRIDE {
    ExtensionsDelegate::Get(browser_context_)->LaunchApp(extension_->id());
  }

  scoped_refptr<const extensions::Extension> extension_;
  content::BrowserContext* browser_context_;
  extensions::IconImage icon_image_;

  DISALLOW_COPY_AND_ASSIGN(AppItem);
};

}  // namespace

ExtensionAppModelBuilder::ExtensionAppModelBuilder(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
}

ExtensionAppModelBuilder::~ExtensionAppModelBuilder() {
}

void ExtensionAppModelBuilder::PopulateApps(app_list::AppListModel* model) {
  ExtensionsDelegate* bridge = ExtensionsDelegate::Get(browser_context_);
  const extensions::ExtensionSet& extensions = bridge->GetInstalledExtensions();
  for (extensions::ExtensionSet::const_iterator iter = extensions.begin();
       iter != extensions.end();
       ++iter) {
    // TODO(mukai): use chrome/browser/extension_ui_util.
    if ((*iter)->ShouldDisplayInAppLauncher()) {
      model->AddItem(scoped_ptr<app_list::AppListItem>(
          new AppItem(*iter, browser_context_)));
    }
  }
}

}  // namespace athena
