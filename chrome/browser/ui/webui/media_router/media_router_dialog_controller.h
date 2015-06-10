// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_DIALOG_CONTROLLER_H_

#include "base/macros.h"
#include "chrome/browser/media/router/create_session_request.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace media_router {

// An instance of this class is tied to a WebContents known as the initiator,
// and is lazily created when a Media Router dialog needs to be shown.
// The MediaRouterDialogController allows creating, querying, and removing a
// Media Router dialog modal to the initiator WebContents.
// This class is not thread safe and must be called on the UI thread.
class MediaRouterDialogController
    : public content::WebContentsUserData<MediaRouterDialogController> {
 public:
  ~MediaRouterDialogController() override;

  // Gets a reference to the MediaRouterDialogController associated with
  // |web_contents|, creating one if it does not exist. The returned pointer is
  // guaranteed to be non-null.
  static MediaRouterDialogController* GetOrCreateForWebContents(
      content::WebContents* web_contents);

  // Shows the media router dialog modal to the initiator WebContents.
  // Creates the dialog if it did not exist prior to this call.
  // If the dialog already exists, brings the dialog to the front.
  // Returns WebContents for the media router dialog.
  content::WebContents* ShowMediaRouterDialog();

  // Creates a Media Router modal dialog using the initiator and parameters
  // specified in |request|. If the dialog already exists, brings the dialog
  // to the front, but does not change the dialog with |request|.
  // Returns WebContents for the media router dialog if a dialog was created.
  // Otherwise returns nullptr and |request| is deleted.
  content::WebContents* ShowMediaRouterDialogForPresentation(
      scoped_ptr<CreateSessionRequest> request);

  // Returns the media router dialog WebContents.
  // Returns nullptr if there is no dialog.
  content::WebContents* GetMediaRouterDialog() const;

  // Closes the media router dialog. This will destroy the dialog WebContents.
  // It is an error to call this function if there is currently no dialog.
  void CloseMediaRouterDialog();

 private:
  class DialogWebContentsObserver;
  class InitiatorWebContentsObserver;
  friend class content::WebContentsUserData<MediaRouterDialogController>;

  // Use MediaRouterDialogController::CreateForWebContents() to create an
  // instance.
  explicit MediaRouterDialogController(content::WebContents* web_contents);

  // Creates a new media router dialog modal to |initiator_|.
  void CreateMediaRouterDialog();

  // Resets this dialog controller to an empty state.
  void Reset();

  // Invoked when the dialog WebContents has navigated.
  void OnDialogNavigated(const content::LoadCommittedDetails& details);

  void PopulateDialog(content::WebContents* media_router_dialog);

  scoped_ptr<InitiatorWebContentsObserver> initiator_observer_;
  scoped_ptr<DialogWebContentsObserver> dialog_observer_;

  content::WebContents* const initiator_;

  // True if the controller is waiting for a new media router dialog to be
  // created.
  bool media_router_dialog_pending_;

  // Data for dialogs created under a Presentation API context.
  // Passed from the caller of ShowMediaRouterDialogForPresentation(), and
  // passed to the MediaRouterUI when it is initialized.
  scoped_ptr<CreateSessionRequest> presentation_request_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterDialogController);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_DIALOG_CONTROLLER_H_
