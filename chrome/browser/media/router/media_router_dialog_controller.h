// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_DIALOG_CONTROLLER_H_

#include "base/macros.h"
#include "chrome/browser/media/router/create_presentation_session_request.h"

namespace content {
class WebContents;
}  // namespace content

namespace media_router {

// An implementation of this interface is tied to a WebContents known as the
// initiator, and is lazily created when a Media Router dialog needs to be
// shown. The MediaRouterDialogController allows creating, querying, and
// removing a Media Router dialog modal to the initiator WebContents.
// This class is not thread safe and must be called on the UI thread.
class MediaRouterDialogController {
 public:
  virtual ~MediaRouterDialogController() = default;

  // Gets a reference to the MediaRouterDialogController associated with
  // |web_contents|, creating one if it does not exist. The returned pointer is
  // guaranteed to be non-null.
  static MediaRouterDialogController* GetOrCreateForWebContents(
      content::WebContents* web_contents);

  // Creates a Media Router modal dialog using the initiator and parameters
  // specified in |request|. If the dialog already exists, brings the dialog
  // to the front, but does not change the dialog with |request|.
  // Returns WebContents for the media router dialog if a dialog was created.
  // Otherwise returns false and |request| is deleted.
  virtual bool ShowMediaRouterDialogForPresentation(
      scoped_ptr<CreatePresentationSessionRequest> request) = 0;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_MEDIA_ROUTER_DIALOG_CONTROLLER_H_
