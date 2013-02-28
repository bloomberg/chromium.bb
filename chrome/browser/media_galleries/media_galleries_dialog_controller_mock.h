// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_DIALOG_CONTROLLER_MOCK_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_DIALOG_CONTROLLER_MOCK_H_

#include "chrome/browser/media_galleries/media_galleries_dialog_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chrome {

class MediaGalleriesDialogControllerMock
    : public MediaGalleriesDialogController {
 public:
  MediaGalleriesDialogControllerMock();
  virtual ~MediaGalleriesDialogControllerMock();

  MOCK_CONST_METHOD0(GetHeader, string16());
  MOCK_CONST_METHOD0(GetSubtext, string16());
  MOCK_CONST_METHOD0(HasPermittedGalleries, bool());
  MOCK_CONST_METHOD0(permissions, const KnownGalleryPermissions&());
  MOCK_METHOD0(web_contents, content::WebContents*());

  MOCK_METHOD0(OnAddFolderClicked, void());
  MOCK_METHOD2(DidToggleGallery, void(const MediaGalleryPrefInfo* pref_info,
                                      bool enabled));
  MOCK_METHOD1(DialogFinished, void(bool));
};

}  // namespace

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_DIALOG_CONTROLLER_MOCK_H_
