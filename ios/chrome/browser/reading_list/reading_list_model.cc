// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/reading_list_model.h"

ReadingListModel::ReadingListModel() {}
ReadingListModel::~ReadingListModel() {}

// Observer methods.
void ReadingListModel::AddObserver(ReadingListModelObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
  if (loaded()) {
    observer->ReadingListModelLoaded(this);
  }
}

void ReadingListModel::RemoveObserver(ReadingListModelObserver* observer) {
  observers_.RemoveObserver(observer);
}
