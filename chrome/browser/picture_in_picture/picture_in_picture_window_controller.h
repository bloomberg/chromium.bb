// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

class OverlayWindow;

// Class for Picture in Picture window controllers. This is currently tied to a
// WebContents |initiator| and created when a Picture in Picture window is to
// be shown. This allows creation of a single window for a initiator
// WebContents.
// TODO(apacible): Determine expected window behaviour and update controller
// to reflect decisions.
class PictureInPictureWindowController
    : public content::WebContentsUserData<PictureInPictureWindowController> {
 public:
  ~PictureInPictureWindowController() override;
  // Gets a reference to the controller associated with |initiator| and creates
  // one if it does not exist. The returned pointer is guaranteed to be
  // non-null.
  static PictureInPictureWindowController* GetOrCreateForWebContents(
      content::WebContents* initiator);

  void Show();
  void Close();

 private:
  friend class content::WebContentsUserData<PictureInPictureWindowController>;

  // Use PictureInPictureWindowController::GetOrCreateForWebContents() to
  // create an instance.
  explicit PictureInPictureWindowController(content::WebContents* initiator);

  content::WebContents* const initiator_;
  std::unique_ptr<OverlayWindow> window_;

  DISALLOW_COPY_AND_ASSIGN(PictureInPictureWindowController);
};

#endif  // CHROME_BROWSER_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_H_
