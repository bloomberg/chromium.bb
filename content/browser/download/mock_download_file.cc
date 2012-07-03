// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/mock_download_file.h"

using ::testing::_;
using ::testing::Return;

MockDownloadFile::MockDownloadFile() {
  // This is here because |Initialize()| is normally called right after
  // construction.
  ON_CALL(*this, Initialize())
      .WillByDefault(Return(content::DOWNLOAD_INTERRUPT_REASON_NONE));
}

MockDownloadFile::~MockDownloadFile() {
}
