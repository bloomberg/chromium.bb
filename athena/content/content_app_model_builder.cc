// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/public/content_app_model_builder.h"

#include "athena/activity/public/activity_factory.h"
#include "athena/activity/public/activity_manager.h"
#include "athena/extensions/public/extensions_delegate.h"
#include "extensions/browser/extension_icon_image.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_model.h"

namespace athena {

namespace {

gfx::ImageSkia CreateFlatColorImage(SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(extension_misc::EXTENSION_ICON_MEDIUM,
                        extension_misc::EXTENSION_ICON_MEDIUM);
  bitmap.eraseColor(color);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

// Same dummy item.
class DummyItem : public app_list::AppListItem {
 public:
  DummyItem(const std::string& id,
            const GURL& url,
            SkColor color,
            content::BrowserContext* browser_context)
      : app_list::AppListItem(id),
        url_(url),
        browser_context_(browser_context) {

    SetIcon(CreateFlatColorImage(color), false /* has_shadow */);
    SetName(id);
  }

 private:
  // Overridden from app_list::AppListItem:
  virtual void Activate(int event_flags) OVERRIDE {
    ActivityManager::Get()->AddActivity(
        ActivityFactory::Get()->CreateWebActivity(browser_context_, url_));
  }

  GURL url_;
  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(DummyItem);
};

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
                    // TODO(mukai): better default icon
                    CreateFlatColorImage(SK_ColorBLACK),
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

ContentAppModelBuilder::ContentAppModelBuilder(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
}

ContentAppModelBuilder::~ContentAppModelBuilder() {
}

void ContentAppModelBuilder::PopulateApps(app_list::AppListModel* model) {
  ExtensionsDelegate* bridge = ExtensionsDelegate::Get(browser_context_);
  const extensions::ExtensionSet& extensions = bridge->GetInstalledExtensions();
  for (extensions::ExtensionSet::const_iterator iter = extensions.begin();
       iter != extensions.end();
       ++iter) {
    model->AddItem(scoped_ptr<app_list::AppListItem>(
        new AppItem(*iter, browser_context_)));
  }

  model->AddItem(scoped_ptr<app_list::AppListItem>(new DummyItem(
      "mail", GURL("http://gmail.com/"), SK_ColorRED, browser_context_)));
  model->AddItem(scoped_ptr<app_list::AppListItem>(new DummyItem(
      "calendar", GURL("https://calendar.google.com/"),
      SK_ColorBLUE, browser_context_)));
  model->AddItem(scoped_ptr<app_list::AppListItem>(new DummyItem(
      "video", GURL("http://youtube.com/"), SK_ColorGREEN, browser_context_)));
  model->AddItem(scoped_ptr<app_list::AppListItem>(new DummyItem(
      "music", GURL("http://play.google.com/music"),
      SK_ColorYELLOW, browser_context_)));
  model->AddItem(scoped_ptr<app_list::AppListItem>(new DummyItem(
      "contact", GURL("https://www.google.com/contacts"),
      SK_ColorCYAN, browser_context_)));
}

}  // namespace athena
