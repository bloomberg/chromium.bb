// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_MEDIA_ROUTER_MEDIA_ROUTER_DIALOG_CONTROLLER_IMPL_BASE_H_
#define CHROME_BROWSER_UI_MEDIA_ROUTER_MEDIA_ROUTER_DIALOG_CONTROLLER_IMPL_BASE_H_

#include "base/macros.h"
#include "chrome/browser/media/router/media_router_dialog_controller.h"
#include "chrome/browser/ui/media_router/media_router_ui_service.h"

namespace media_router {

class MediaRouterUIBase;

// The base class for desktop implementations of MediaRouterDialogController.
// This class is not thread safe and must be called on the UI thread.
class MediaRouterDialogControllerImplBase : public MediaRouterDialogController,
                                            MediaRouterUIService::Observer {
 public:
  ~MediaRouterDialogControllerImplBase() override;

  static MediaRouterDialogControllerImplBase* GetOrCreateForWebContents(
      content::WebContents* web_contents);
  static MediaRouterDialogControllerImplBase* FromWebContents(
      content::WebContents* web_contents);

  // MediaRouterDialogController:
  void CreateMediaRouterDialog() override;
  void Reset() override;

 protected:
  // Use MediaRouterDialogControllerImplBase::CreateForWebContents() to create
  // an instance.
  explicit MediaRouterDialogControllerImplBase(
      content::WebContents* web_contents);

  // Called by subclasses to initialize |media_router_ui| that they use.
  void InitializeMediaRouterUI(MediaRouterUIBase* media_router_ui);

 private:
  // MediaRouterUIService::Observer:
  void OnServiceDisabled() override;

  // MediaRouterActionController is responsible for showing and hiding the
  // toolbar action. It's owned by MediaRouterUIService and it may be nullptr.
  MediaRouterActionController* GetActionController();

  // |media_router_ui_service_| Service which provides
  // MediaRouterActionController. It outlives |this|.
  MediaRouterUIService* const media_router_ui_service_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterDialogControllerImplBase);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_MEDIA_ROUTER_MEDIA_ROUTER_DIALOG_CONTROLLER_IMPL_BASE_H_
