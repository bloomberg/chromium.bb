// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/home/app_list_view_delegate.h"

#include <string>
#include <vector>

#include "athena/home/public/app_model_builder.h"
#include "athena/home/public/search_controller_factory.h"
#include "athena/strings/grit/athena_strings.h"
#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/search_box_model.h"
#include "ui/app_list/search_controller.h"
#include "ui/app_list/search_provider.h"
#include "ui/app_list/search_result.h"
#include "ui/app_list/speech_ui_model.h"
#include "ui/base/l10n/l10n_util.h"

namespace athena {

AppListViewDelegate::AppListViewDelegate(
    AppModelBuilder* model_builder,
    SearchControllerFactory* search_factory)
    : model_(new app_list::AppListModel),
      speech_ui_(new app_list::SpeechUIModel) {
  model_builder->RegisterAppListModel(model_.get());
  model_->search_box()->SetHintText(
      l10n_util::GetStringUTF16(IDS_ATHENA_SEARCH_BOX_HINT));
  if (search_factory) {
    search_controller_ =
        search_factory->Create(model_->search_box(), model_->results());
  }
}

AppListViewDelegate::~AppListViewDelegate() {
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
  if (search_controller_)
    search_controller_->Start(false);
}

void AppListViewDelegate::StopSearch() {
  if (search_controller_)
    search_controller_->Stop();
}

void AppListViewDelegate::OpenSearchResult(app_list::SearchResult* result,
                                           bool auto_launch,
                                           int event_flags) {
  if (search_controller_)
    search_controller_->OpenResult(result, event_flags);
}

void AppListViewDelegate::InvokeSearchResultAction(
    app_list::SearchResult* result,
    int action_index,
    int event_flags) {
  if (search_controller_)
    search_controller_->InvokeResultAction(result, action_index, event_flags);
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
  return nullptr;
}

std::vector<views::View*> AppListViewDelegate::CreateCustomPageWebViews(
    const gfx::Size& size) {
  return std::vector<views::View*>();
}

void AppListViewDelegate::CustomLauncherPageAnimationChanged(double progress) {
}

void AppListViewDelegate::CustomLauncherPagePopSubpage() {
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
