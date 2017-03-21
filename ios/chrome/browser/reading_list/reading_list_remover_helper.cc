// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/reading_list_remover_helper.h"

#include "components/reading_list/ios/reading_list_model.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/reading_list/reading_list_download_service.h"
#include "ios/chrome/browser/reading_list/reading_list_download_service_factory.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"

namespace reading_list {

ReadingListRemoverHelper::ReadingListRemoverHelper(
    ios::ChromeBrowserState* browser_state)
    : ReadingListModelObserver(), scoped_observer_(this) {
  reading_list_model_ =
      ReadingListModelFactory::GetForBrowserState(browser_state);
  reading_list_download_service_ =
      ReadingListDownloadServiceFactory::GetForBrowserState(browser_state);
}

ReadingListRemoverHelper::~ReadingListRemoverHelper() {}

void ReadingListRemoverHelper::ReadingListModelLoaded(
    const ReadingListModel* model) {
  scoped_observer_.Remove(reading_list_model_);

  bool model_cleared = reading_list_model_->DeleteAllEntries();
  reading_list_download_service_->Clear();
  reading_list_model_ = nullptr;
  if (completion_) {
    // |completion_| may delete this. Do not use the object after this call.
    std::move(completion_).Run(model_cleared);
  }
}

void ReadingListRemoverHelper::ReadingListModelBeingDeleted(
    const ReadingListModel* model) {
  scoped_observer_.Remove(reading_list_model_);
  reading_list_model_ = nullptr;
  if (completion_) {
    // |completion_| may delete this. Do not use the object after this call.
    std::move(completion_).Run(false);
  }
}

void ReadingListRemoverHelper::RemoveAllUserReadingListItemsIOS(
    Callback completion) {
  if (!reading_list_model_) {
    // |completion_| may delete this. Do not use the object after this call.
    std::move(completion_).Run(false);
    return;
  }
  completion_ = std::move(completion);

  // Calls ReadingListModelLoaded if model is already loaded.
  scoped_observer_.Add(reading_list_model_);
}

}  // namespace reading_list
