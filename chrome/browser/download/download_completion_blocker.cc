// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_completion_blocker.h"

#include "base/logging.h"

DownloadCompletionBlocker::DownloadCompletionBlocker()
  : is_complete_(false) {
}

DownloadCompletionBlocker::~DownloadCompletionBlocker() {
}

void DownloadCompletionBlocker::CompleteDownload() {
  DCHECK(!is_complete_);
  is_complete_ = true;
  if (callback_.is_null())
    return;
  callback_.Run();
  callback_.Reset();
}
