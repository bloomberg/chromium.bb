// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/test_download_shelf.h"

TestDownloadShelf::TestDownloadShelf()
    : is_showing_(false) {
}

TestDownloadShelf::~TestDownloadShelf() {
}

bool TestDownloadShelf::IsShowing() const {
  return is_showing_;
}

bool TestDownloadShelf::IsClosing() const {
  return false;
}

Browser* TestDownloadShelf::browser() const {
  return NULL;
}

void TestDownloadShelf::DoAddDownload(content::DownloadItem* download) {
}

void TestDownloadShelf::DoShow() {
  is_showing_ = true;
}

void TestDownloadShelf::DoClose() {
  is_showing_ = false;
}

