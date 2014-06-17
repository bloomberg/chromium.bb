// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/public/content_app_model_builder.h"

#include "apps/shell/browser/shell_extension_system.h"
#include "athena/activity/public/activity_factory.h"
#include "athena/activity/public/activity_manager.h"
#include "extensions/common/extension.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_model.h"

using extensions::ShellExtensionSystem;

namespace athena {

namespace {

const int kIconSize = 64;

ShellExtensionSystem* GetShellExtensionSystem(
    content::BrowserContext* context) {
  return static_cast<ShellExtensionSystem*>(
      extensions::ExtensionSystem::Get(context));
}

gfx::ImageSkia CreateFlatColorImage(SkColor color) {
  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, kIconSize, kIconSize);
  bitmap.allocPixels();
  bitmap.eraseColor(color);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

// Same dummy item.
class DummyItem : public app_list::AppListItem {
 public:
  DummyItem(const std::string& id,
            SkColor color,
            content::BrowserContext* browser_context)
      : app_list::AppListItem(id),
        id_(id),
        browser_context_(browser_context) {

    SetIcon(CreateFlatColorImage(color), false /* has_shadow */);
    SetName(id);
  }

 private:
  // Overridden from app_list::AppListItem:
  virtual void Activate(int event_flags) OVERRIDE {
    ActivityManager::Get()->AddActivity(
        ActivityFactory::Get()->CreateAppActivity(
            browser_context_, id_));
  }

  std::string id_;
  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(DummyItem);
};

class AppItem : public app_list::AppListItem {
 public:
  AppItem(scoped_refptr<extensions::Extension> extension,
          content::BrowserContext* browser_context)
      : app_list::AppListItem(extension->id()),
        extension_(extension),
        browser_context_(browser_context) {
    // TODO(mukai): componentize extension_icon_image and use it.
    SetIcon(CreateFlatColorImage(SK_ColorBLACK), false);
    SetName(extension->name());
  }

 private:
  // Overridden from app_list::AppListItem:
  virtual void Activate(int event_flags) OVERRIDE {
    // TODO(mukai): Pass |extension_| when the extension system supports
    // multiple extensions.
    GetShellExtensionSystem(browser_context_)->LaunchApp();
  }

  scoped_refptr<extensions::Extension> extension_;
  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(AppItem);
};

}  // namespace

ContentAppModelBuilder::ContentAppModelBuilder(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
}

ContentAppModelBuilder::~ContentAppModelBuilder() {
}

void ContentAppModelBuilder::PopulateApps(app_list::AppListModel* model) {
  model->AddItem(scoped_ptr<app_list::AppListItem>(
      new DummyItem("mail", SK_ColorRED, browser_context_)));
  model->AddItem(scoped_ptr<app_list::AppListItem>(
      new DummyItem("calendar", SK_ColorBLUE, browser_context_)));
  model->AddItem(scoped_ptr<app_list::AppListItem>(
      new DummyItem("video", SK_ColorGREEN, browser_context_)));
  model->AddItem(scoped_ptr<app_list::AppListItem>(
      new DummyItem("music", SK_ColorYELLOW, browser_context_)));
  model->AddItem(scoped_ptr<app_list::AppListItem>(
      new DummyItem("contact", SK_ColorCYAN, browser_context_)));

  ShellExtensionSystem* extension_system =
      GetShellExtensionSystem(browser_context_);
  if (extension_system && extension_system->extension()) {
    model->AddItem(scoped_ptr<app_list::AppListItem>(
        new AppItem(extension_system->extension(), browser_context_)));
  }
}

}  // namespace athena
