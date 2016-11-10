// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_DIALOG_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_DIALOG_CONTROLLER_IMPL_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/media/router/media_router_dialog_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

FORWARD_DECLARE_TEST(MediaRouterActionUnitTest, IconPressedState);

class MediaRouterAction;
class MediaRouterActionController;

namespace media_router {

// A desktop implementation of MediaRouterDialogController.
// This class is not thread safe and must be called on the UI thread.
class MediaRouterDialogControllerImpl :
    public content::WebContentsUserData<MediaRouterDialogControllerImpl>,
    public MediaRouterDialogController {
 public:
  ~MediaRouterDialogControllerImpl() override;

  static MediaRouterDialogControllerImpl* GetOrCreateForWebContents(
      content::WebContents* web_contents);

  // Returns the media router dialog WebContents.
  // Returns nullptr if there is no dialog.
  content::WebContents* GetMediaRouterDialog() const;

  // Sets the action to notify when a dialog gets shown or hidden.
  void SetMediaRouterAction(const base::WeakPtr<MediaRouterAction>& action);

  // MediaRouterDialogController:
  bool IsShowingMediaRouterDialog() const override;

  void UpdateMaxDialogSize();

  MediaRouterAction* action() { return action_.get(); }

 private:
  class DialogWebContentsObserver;
  friend class content::WebContentsUserData<MediaRouterDialogControllerImpl>;
  FRIEND_TEST_ALL_PREFIXES(::MediaRouterActionUnitTest, IconPressedState);

  // Use MediaRouterDialogControllerImpl::CreateForWebContents() to create an
  // instance.
  explicit MediaRouterDialogControllerImpl(content::WebContents* web_contents);

  // MediaRouterDialogController:
  void CreateMediaRouterDialog() override;
  void CloseMediaRouterDialog() override;
  void Reset() override;

  // Invoked when the dialog WebContents has navigated.
  void OnDialogNavigated(const content::LoadCommittedDetails& details);

  void PopulateDialog(content::WebContents* media_router_dialog);

  std::unique_ptr<DialogWebContentsObserver> dialog_observer_;

  // True if the controller is waiting for a new media router dialog to be
  // created.
  bool media_router_dialog_pending_;

  // |action_| refers to the MediaRouterAction on the toolbar, rather than
  // overflow menu. A MediaRouterAction is always created for the toolbar
  // first. Any subsequent creations for the overflow menu will not be set as
  // |action_|.
  // The lifetime of |action_| is dependent on the creation and destruction of
  // a browser window. The overflow menu's MediaRouterAction is only created
  // when the overflow menu is opened and destroyed when the menu is closed.
  base::WeakPtr<MediaRouterAction> action_;

  // |action_controller_| is responsible for showing and hiding the toolbar
  // action. It's owned by MediaRouterUIService, which outlives |this|.
  MediaRouterActionController* action_controller_;

  base::WeakPtrFactory<MediaRouterDialogControllerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterDialogControllerImpl);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_DIALOG_CONTROLLER_IMPL_H_
