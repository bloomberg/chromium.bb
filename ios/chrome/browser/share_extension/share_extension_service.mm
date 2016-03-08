// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/scoped_nsobject.h"
#include "ios/chrome/browser/reading_list/reading_list_model.h"
#include "ios/chrome/browser/share_extension/share_extension_item_receiver.h"
#include "ios/chrome/browser/share_extension/share_extension_service.h"

ShareExtensionService::ShareExtensionService(ReadingListModel* model)
    : reading_list_model_(model) {
  DCHECK(model);
}

ShareExtensionService::~ShareExtensionService() {}

void ShareExtensionService::Initialize() {
  reading_list_model_->AddObserver(this);
}

void ShareExtensionService::Shutdown() {
  [[ShareExtensionItemReceiver sharedInstance] shutdown];
  reading_list_model_->RemoveObserver(this);
}

void ShareExtensionService::ReadingListModelLoaded(
    const ReadingListModel* model) {
  [[ShareExtensionItemReceiver sharedInstance]
      setReadingListModel:reading_list_model_];
}
