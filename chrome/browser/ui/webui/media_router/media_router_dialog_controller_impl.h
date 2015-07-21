// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_DIALOG_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_DIALOG_CONTROLLER_IMPL_H_

#include "base/macros.h"
#include "chrome/browser/media/router/media_router_dialog_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace media_router {

// A desktop implementation of MediaRouterDialogController.
// This class is not thread safe and must be called on the UI thread.
class MediaRouterDialogControllerImpl
    : public content::WebContentsUserData<MediaRouterDialogControllerImpl>,
      public MediaRouterDialogController {
 public:
  ~MediaRouterDialogControllerImpl() override;

  static MediaRouterDialogControllerImpl* GetOrCreateForWebContents(
      content::WebContents* web_contents);

  // SuperClass:
  bool ShowMediaRouterDialogForPresentation(
      scoped_ptr<CreatePresentationSessionRequest> request) override;

  // Shows the media router dialog modal to the initiator WebContents.
  // Creates the dialog if it did not exist prior to this call.
  // If the dialog already exists, brings the dialog to the front.
  // Returns WebContents for the media router dialog.
  content::WebContents* ShowMediaRouterDialog();

  // Returns the media router dialog WebContents.
  // Returns nullptr if there is no dialog.
  content::WebContents* GetMediaRouterDialog() const;

  // Closes the media router dialog. This will destroy the dialog WebContents.
  // It is an error to call this function if there is currently no dialog.
  void CloseMediaRouterDialog();

 private:
  class DialogWebContentsObserver;
  class InitiatorWebContentsObserver;
  friend class content::WebContentsUserData<MediaRouterDialogControllerImpl>;

  // Use MediaRouterDialogControllerImpl::CreateForWebContents() to create an
  // instance.
  explicit MediaRouterDialogControllerImpl(content::WebContents* web_contents);

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
  scoped_ptr<CreatePresentationSessionRequest> presentation_request_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterDialogControllerImpl);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_DIALOG_CONTROLLER_IMPL_H_
