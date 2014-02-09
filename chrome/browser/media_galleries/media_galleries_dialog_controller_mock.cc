// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/media_galleries_dialog_controller_mock.h"

#include "base/bind.h"
#include "base/bind_helpers.h"

using ::testing::Return;

namespace {
MediaGalleriesDialog* NullDialog(MediaGalleriesDialogController*) {
  return NULL;
}

}  // namespace

MediaGalleriesDialogControllerMock::MediaGalleriesDialogControllerMock(
    const extensions::Extension& extension)
    : MediaGalleriesDialogController(
          extension, NULL,
          base::Bind(&NullDialog),
          base::Bind(&base::DoNothing)) {
  EXPECT_CALL(*this, GetHeader()).
      WillRepeatedly(Return(base::string16()));
  EXPECT_CALL(*this, GetSubtext()).
      WillRepeatedly(Return(base::string16()));
}

MediaGalleriesDialogControllerMock::~MediaGalleriesDialogControllerMock() {
}
