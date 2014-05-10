// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mock_download_item.h"

namespace content {

MockDownloadItem::MockDownloadItem() {}

MockDownloadItem::~MockDownloadItem() {
  FOR_EACH_OBSERVER(Observer, observers_, OnDownloadDestroyed(this));
}

void MockDownloadItem::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MockDownloadItem::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void MockDownloadItem::NotifyObserversDownloadOpened() {
  FOR_EACH_OBSERVER(Observer, observers_, OnDownloadOpened(this));
}

void MockDownloadItem::NotifyObserversDownloadRemoved() {
  FOR_EACH_OBSERVER(Observer, observers_, OnDownloadRemoved(this));
}

void MockDownloadItem::NotifyObserversDownloadUpdated() {
  FOR_EACH_OBSERVER(Observer, observers_, OnDownloadUpdated(this));
}

}
