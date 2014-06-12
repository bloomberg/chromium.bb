// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/public/content_app_model_builder.h"

#include "athena/activity/public/activity_factory.h"
#include "athena/activity/public/activity_manager.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_model.h"

namespace athena {

namespace {

const int kIconSize = 64;

// Same dummy item.
class DummyItem : public app_list::AppListItem {
 public:
  DummyItem(const std::string& id,
            SkColor color,
            content::BrowserContext* browser_context)
      : app_list::AppListItem(id),
        id_(id),
        browser_context_(browser_context) {

    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, kIconSize, kIconSize);
    bitmap.allocPixels();
    bitmap.eraseColor(color);
    SetIcon(gfx::ImageSkia::CreateFrom1xBitmap(bitmap), false /* has_shadow */);
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
}

}  // namespace athena
