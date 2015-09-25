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

  // |action| must always be non-null.
  void SetMediaRouterAction(const base::WeakPtr<MediaRouterAction>& action);

  // MediaRouterDialogController:
  bool IsShowingMediaRouterDialog() const override;

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

  scoped_ptr<DialogWebContentsObserver> dialog_observer_;

  // True if the controller is waiting for a new media router dialog to be
  // created.
  bool media_router_dialog_pending_;

  base::WeakPtr<MediaRouterAction> action_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterDialogControllerImpl);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_WEBUI_MEDIA_ROUTER_MEDIA_ROUTER_DIALOG_CONTROLLER_IMPL_H_
