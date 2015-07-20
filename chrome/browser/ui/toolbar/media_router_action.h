// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_MEDIA_ROUTER_ACTION_H_
#define CHROME_BROWSER_UI_TOOLBAR_MEDIA_ROUTER_ACTION_H_

#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"

class Browser;
class MediaRouterActionPlatformDelegate;

namespace media_router {
class MediaRouterDialogController;
}  // namespace media_router

// The class for the Media Router component action that will be shown in
// the toolbar.
class MediaRouterAction : public ToolbarActionViewController {
 public:
  explicit MediaRouterAction(Browser* browser);
  ~MediaRouterAction() override;

  // ToolbarActionViewController implementation.
  const std::string& GetId() const override;
  void SetDelegate(ToolbarActionViewDelegate* delegate) override;
  gfx::Image GetIcon(content::WebContents* web_contents,
                     const gfx::Size& size) override;
  base::string16 GetActionName() const override;
  base::string16 GetAccessibleName(content::WebContents* web_contents)
      const override;
  base::string16 GetTooltip(content::WebContents* web_contents)
      const override;
  bool IsEnabled(content::WebContents* web_contents) const override;
  bool WantsToRun(content::WebContents* web_contents) const override;
  bool HasPopup(content::WebContents* web_contents) const override;
  void HidePopup() override;
  gfx::NativeView GetPopupNativeView() override;
  ui::MenuModel* GetContextMenu() override;
  bool CanDrag() const override;
  bool ExecuteAction(bool by_user) override;
  void UpdateState() override;
  bool DisabledClickOpensMenu() const override;

 private:
  // Returns a reference to the MediaRouterDialogController associated with
  // |delegate_|'s current WebContents. Guaranteed to be non-null.
  // |delegate_| and its current WebContents must not be null.
  media_router::MediaRouterDialogController* GetMediaRouterDialogController();

  const std::string id_;
  const base::string16 name_;

  // Cached icons.
  gfx::Image media_router_idle_icon_;

  ToolbarActionViewDelegate* delegate_;

  // The delegate to handle platform-specific implementations.
  scoped_ptr<MediaRouterActionPlatformDelegate> platform_delegate_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterAction);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_MEDIA_ROUTER_ACTION_H_
