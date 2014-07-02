// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/test/test_app_model_builder.h"

#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_model.h"

namespace athena {

namespace {

const int kIconSize = 64;

class DummyItem : public app_list::AppListItem {
 public:
  enum Type {
    DUMMY_MAIL,
    DUMMY_CALENDAR,
    DUMMY_VIDEO,
    DUMMY_MUSIC,
    DUMMY_CONTACT,
    LAST_TYPE,
  };

  static std::string GetTitle(Type type) {
    switch (type) {
      case DUMMY_MAIL:
        return "mail";
      case DUMMY_CALENDAR:
        return "calendar";
      case DUMMY_VIDEO:
        return "video";
      case DUMMY_MUSIC:
        return "music";
      case DUMMY_CONTACT:
        return "contact";
      case LAST_TYPE:
        break;
    }
    NOTREACHED();
    return "";
  }

  static std::string GetId(Type type) {
    return std::string("id-") + GetTitle(type);
  }

  explicit DummyItem(Type type)
      : app_list::AppListItem(GetId(type)),
        type_(type) {
    SetIcon(GetIcon(), false /* has_shadow */);
    SetName(GetTitle(type_));
  }

 private:
  gfx::ImageSkia GetIcon() const {
    SkColor color = SK_ColorWHITE;
    switch (type_) {
      case DUMMY_MAIL:
        color = SK_ColorRED;
        break;
      case DUMMY_CALENDAR:
        color = SK_ColorBLUE;
        break;
      case DUMMY_VIDEO:
        color = SK_ColorGREEN;
        break;
      case DUMMY_MUSIC:
        color = SK_ColorYELLOW;
        break;
      case DUMMY_CONTACT:
        color = SK_ColorCYAN;
        break;
      case LAST_TYPE:
        NOTREACHED();
        break;
    }
    SkBitmap bitmap;
    bitmap.allocN32Pixels(kIconSize, kIconSize);
    bitmap.eraseColor(color);
    return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  }

  Type type_;

  DISALLOW_COPY_AND_ASSIGN(DummyItem);
};

}  // namespace

TestAppModelBuilder::TestAppModelBuilder() {
}

TestAppModelBuilder::~TestAppModelBuilder() {
}

void TestAppModelBuilder::PopulateApps(app_list::AppListModel* model) {
  for (int i = 0; i < static_cast<int>(DummyItem::LAST_TYPE); ++i) {
    model->AddItem(scoped_ptr<app_list::AppListItem>(
        new DummyItem(static_cast<DummyItem::Type>(i))));
  }
}

}  // namespace athena
