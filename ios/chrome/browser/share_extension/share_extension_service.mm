// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/scoped_nsobject.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/reading_list/ios/reading_list_model.h"
#include "ios/chrome/browser/share_extension/share_extension_item_receiver.h"
#include "ios/chrome/browser/share_extension/share_extension_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

ShareExtensionService::ShareExtensionService(
    bookmarks::BookmarkModel* bookmark_model,
    ReadingListModel* reading_list_model)
    : reading_list_model_(reading_list_model),
      reading_list_model_loaded_(false),
      bookmark_model_(bookmark_model),
      bookmark_model_loaded_(false) {
  DCHECK(bookmark_model);
  DCHECK(reading_list_model);
}

ShareExtensionService::~ShareExtensionService() {}

void ShareExtensionService::Initialize() {
  bookmark_model_->AddObserver(this);
  if (bookmark_model_->loaded()) {
    bookmark_model_loaded_ = true;
    this->AnyModelLoaded();
  }
  reading_list_model_->AddObserver(this);
}

void ShareExtensionService::Shutdown() {
  [[ShareExtensionItemReceiver sharedInstance] shutdown];
  reading_list_model_->RemoveObserver(this);
  reading_list_model_loaded_ = false;
  bookmark_model_->RemoveObserver(this);
  bookmark_model_loaded_ = false;
}

void ShareExtensionService::ReadingListModelLoaded(
    const ReadingListModel* model) {
  reading_list_model_loaded_ = true;
  this->AnyModelLoaded();
}

void ShareExtensionService::BookmarkModelLoaded(bookmarks::BookmarkModel* model,
                                                bool ids_reassigned) {
  bookmark_model_loaded_ = true;
  this->AnyModelLoaded();
}

void ShareExtensionService::AnyModelLoaded() {
  if (reading_list_model_loaded_ && bookmark_model_loaded_) {
    [[ShareExtensionItemReceiver sharedInstance]
        setBookmarkModel:bookmark_model_
        readingListModel:reading_list_model_];
  }
}
