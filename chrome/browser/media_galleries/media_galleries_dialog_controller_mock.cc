// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/media_galleries_dialog_controller_mock.h"

using ::testing::Return;

namespace chrome {

MediaGalleriesDialogControllerMock::MediaGalleriesDialogControllerMock() {
  EXPECT_CALL(*this, GetHeader()).
      WillRepeatedly(Return(string16()));
  EXPECT_CALL(*this, GetSubtext()).
      WillRepeatedly(Return(string16()));
}

MediaGalleriesDialogControllerMock::~MediaGalleriesDialogControllerMock() {
}

}  // namespace
