// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_shelf.h"

DownloadShelf::DownloadShelf()
    : should_show_on_unhide_(false),
      is_hidden_(false) {}

void DownloadShelf::AddDownload(content::DownloadItem* download) {
  if (is_hidden_)
    Unhide();
  Show();
  DoAddDownload(download);
}

void DownloadShelf::Show() {
  if (is_hidden_) {
    should_show_on_unhide_ = true;
    return;
  }
  DoShow();
}

void DownloadShelf::Close() {
  if (is_hidden_) {
    should_show_on_unhide_ = false;
    return;
  }
  DoClose();
}

void DownloadShelf::Hide() {
  if (is_hidden_)
    return;
  is_hidden_ = true;
  if (IsShowing()) {
    should_show_on_unhide_ = true;
    DoClose();
  }
}

void DownloadShelf::Unhide() {
  if (!is_hidden_)
    return;
  is_hidden_ = false;
  if (should_show_on_unhide_) {
    should_show_on_unhide_ = false;
    DoShow();
  }
}
