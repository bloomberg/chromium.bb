// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/home/app_list_view_delegate.h"

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_item_list.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/search_box_model.h"
#include "ui/app_list/search_result.h"
#include "ui/app_list/speech_ui_model.h"
#include "ui/gfx/image/image_skia.h"

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
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, kIconSize, kIconSize);
    bitmap.allocPixels();
    bitmap.eraseColor(color);
    return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  }

  Type type_;

  DISALLOW_COPY_AND_ASSIGN(DummyItem);
};

}  // namespace

AppListViewDelegate::AppListViewDelegate()
    : model_(new app_list::AppListModel),
      speech_ui_(new app_list::SpeechUIModel(
          app_list::SPEECH_RECOGNITION_OFF)) {
  PopulateApps();
  // TODO(mukai): get the text from the resources.
  model_->search_box()->SetHintText(base::ASCIIToUTF16("Search"));
}

AppListViewDelegate::~AppListViewDelegate() {
}

void AppListViewDelegate::PopulateApps() {
  for (int i = 0; i < static_cast<int>(DummyItem::LAST_TYPE); ++i) {
    model_->AddItem(scoped_ptr<app_list::AppListItem>(
        new DummyItem(static_cast<DummyItem::Type>(i))));
  }
}

bool AppListViewDelegate::ForceNativeDesktop() const {
  return false;
}

void AppListViewDelegate::SetProfileByPath(const base::FilePath& profile_path) {
  // Nothing needs to be done.
}

app_list::AppListModel* AppListViewDelegate::GetModel() {
  return model_.get();
}

app_list::SpeechUIModel* AppListViewDelegate::GetSpeechUI() {
  return speech_ui_.get();
}

void AppListViewDelegate::GetShortcutPathForApp(
    const std::string& app_id,
    const base::Callback<void(const base::FilePath&)>& callback) {
  // Windows only, nothing is necessary.
}

void AppListViewDelegate::StartSearch() {
  // TODO(mukai): implement this.
}

void AppListViewDelegate::StopSearch() {
  // TODO(mukai): implement this.
}

void AppListViewDelegate::OpenSearchResult(app_list::SearchResult* result,
                                           bool auto_launch,
                                           int event_flags) {
  // TODO(mukai): implement this.
}

void AppListViewDelegate::InvokeSearchResultAction(
    app_list::SearchResult* result,
    int action_index,
    int event_flags) {
  // TODO(mukai): implement this.
}

base::TimeDelta AppListViewDelegate::GetAutoLaunchTimeout() {
  // Used by voice search, nothing needs to be done for now.
  return base::TimeDelta();
}

void AppListViewDelegate::AutoLaunchCanceled() {
  // Used by voice search, nothing needs to be done for now.
}

void AppListViewDelegate::ViewInitialized() {
  // Nothing needs to be done.
}

void AppListViewDelegate::Dismiss() {
  // Nothing needs to be done.
}

void AppListViewDelegate::ViewClosing() {
  // Nothing needs to be done.
}

gfx::ImageSkia AppListViewDelegate::GetWindowIcon() {
  return gfx::ImageSkia();
}

void AppListViewDelegate::OpenSettings() {
  // Nothing needs to be done for now.
  // TODO(mukai): should invoke the settings app.
}

void AppListViewDelegate::OpenHelp() {
  // Nothing needs to be done for now.
  // TODO(mukai): should invoke the help app.
}

void AppListViewDelegate::OpenFeedback() {
  // Nothing needs to be done for now.
  // TODO(mukai): should invoke the feedback app.
}

void AppListViewDelegate::ToggleSpeechRecognition() {
  // Nothing needs to be done.
}

void AppListViewDelegate::ShowForProfileByPath(
    const base::FilePath& profile_path) {
  // Nothing needs to be done.
}

views::View* AppListViewDelegate::CreateStartPageWebView(
    const gfx::Size& size) {
  return NULL;
}

bool AppListViewDelegate::IsSpeechRecognitionEnabled() {
  return false;
}

const app_list::AppListViewDelegate::Users&
AppListViewDelegate::GetUsers() const {
  return users_;
}

bool AppListViewDelegate::ShouldCenterWindow() const {
  return true;
}

}  // namespace athena
