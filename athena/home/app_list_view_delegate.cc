// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/home/app_list_view_delegate.h"

#include <string>
#include <vector>

#include "athena/home/public/app_model_builder.h"
#include "athena/strings/grit/athena_strings.h"
#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/search_box_model.h"
#include "ui/app_list/search_provider.h"
#include "ui/app_list/search_result.h"
#include "ui/app_list/speech_ui_model.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/controls/image_view.h"

namespace athena {

AppListViewDelegate::AppListViewDelegate(AppModelBuilder* model_builder)
    : model_(new app_list::AppListModel),
      speech_ui_(new app_list::SpeechUIModel) {
  model_builder->PopulateApps(model_.get());
  model_->search_box()->SetHintText(
      l10n_util::GetStringUTF16(IDS_ATHENA_SEARCH_BOX_HINT));
}

AppListViewDelegate::~AppListViewDelegate() {
  for (size_t i = 0; i < search_providers_.size(); ++i)
    search_providers_[i]->set_result_changed_callback(base::Closure());
}

void AppListViewDelegate::RegisterSearchProvider(
    app_list::SearchProvider* search_provider) {
  // Right now we allow only one provider.
  // TODO(mukai): port app-list's mixer and remove this restriction.
  DCHECK(search_providers_.empty());
  search_provider->set_result_changed_callback(base::Bind(
      &AppListViewDelegate::SearchResultChanged, base::Unretained(this)));
  search_providers_.push_back(search_provider);
}

void AppListViewDelegate::SearchResultChanged() {
  // TODO(mukai): port app-list's Mixer to reorder the results properly.
  app_list::SearchProvider* search_provider = search_providers_[0];
  std::vector<app_list::SearchResult*> results;
  search_provider->ReleaseResult(&results);
  if (results.empty()) {
    model_->results()->DeleteAll();
  } else {
    for (size_t i = 0; i < results.size(); ++i)
      model_->results()->Add(results[i]);
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
  for (size_t i = 0; i < search_providers_.size(); ++i)
    search_providers_[i]->Start(model_->search_box()->text());
}

void AppListViewDelegate::StopSearch() {
  for (size_t i = 0; i < search_providers_.size(); ++i)
    search_providers_[i]->Stop();
}

void AppListViewDelegate::OpenSearchResult(app_list::SearchResult* result,
                                           bool auto_launch,
                                           int event_flags) {
  result->Open(event_flags);
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
  // A static image of the logo. This needs to support dynamic Doodles
  // eventually.
  views::ImageView* logo_image = new views::ImageView();
  logo_image->SetImage(ui::ResourceBundle::GetSharedInstance().
                       GetImageSkiaNamed(IDR_LOCAL_NTP_IMAGES_LOGO_PNG));
  logo_image->SetHorizontalAlignment(views::ImageView::CENTER);
  logo_image->SetVerticalAlignment(views::ImageView::CENTER);
  return logo_image;
}

std::vector<views::View*> AppListViewDelegate::CreateCustomPageWebViews(
    const gfx::Size& size) {
  return std::vector<views::View*>();
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
