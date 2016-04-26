// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ZOOM_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ZOOM_BUBBLE_VIEW_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/extension_icon_image.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"

class FullscreenController;

namespace content {
class NotificationDetails;
class NotificationSource;
class WebContents;
}

namespace views {
class ImageButton;
}  // namespace views

// View used to display the zoom percentage when it has changed.
class ZoomBubbleView : public LocationBarBubbleDelegateView,
                       public views::ButtonListener,
                       public ImmersiveModeController::Observer,
                       public extensions::IconImage::Observer {
 public:
  // Shows the bubble and automatically closes it after a short time period if
  // |reason| is AUTOMATIC.
  static void ShowBubble(content::WebContents* web_contents,
                         DisplayReason reason);

  // Closes the showing bubble (if one exists).
  static void CloseCurrentBubble();

  // Returns the zoom bubble if the zoom bubble is showing. Returns NULL
  // otherwise.
  static ZoomBubbleView* GetZoomBubble();

 private:
  FRIEND_TEST_ALL_PREFIXES(ZoomBubbleBrowserTest, ImmersiveFullscreen);

  // Stores information about the extension that initiated the zoom change, if
  // any.
  struct ZoomBubbleExtensionInfo {
    ZoomBubbleExtensionInfo();
    ~ZoomBubbleExtensionInfo();

    // The unique id of the extension, which is used to find the correct
    // extension after clicking on the image button in the zoom bubble.
    std::string id;

    // The name of the extension, which appears in the tooltip of the image
    // button in the zoom bubble.
    std::string name;

    // An image of the extension's icon, which appears in the zoom bubble as an
    // image button.
    std::unique_ptr<const extensions::IconImage> icon_image;
  };

  ZoomBubbleView(views::View* anchor_view,
                 content::WebContents* web_contents,
                 DisplayReason reason,
                 ImmersiveModeController* immersive_mode_controller);
  ~ZoomBubbleView() override;

  // LocationBarBubbleDelegateView:
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void Init() override;
  void WindowClosing() override;
  void CloseBubble() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // ImmersiveModeController::Observer:
  void OnImmersiveRevealStarted() override;
  void OnImmersiveModeControllerDestroyed() override;

  // extensions::IconImage::Observer:
  void OnExtensionIconImageChanged(extensions::IconImage* /* image */) override;

  // Refreshes the bubble by changing the zoom percentage appropriately and
  // resetting the timer if necessary.
  void Refresh();

  // Sets information about the extension that initiated the zoom change.
  // Calling this method asserts that the extension |extension| did initiate
  // the zoom change.
  void SetExtensionInfo(const extensions::Extension* extension);

  // Starts a timer which will close the bubble if |auto_close_| is true.
  void StartTimerIfNecessary();

  // Stops the auto-close timer.
  void StopTimer();

  ZoomBubbleExtensionInfo extension_info_;

  // Singleton instance of the zoom bubble. The zoom bubble can only be shown on
  // the active browser window, so there is no case in which it will be shown
  // twice at the same time.
  static ZoomBubbleView* zoom_bubble_;

  // Timer used to auto close the bubble.
  base::OneShotTimer timer_;

  // Image button in the zoom bubble that will show the |extension_icon_| image
  // if an extension initiated the zoom change, and links to that extension at
  // "chrome://extensions".
  views::ImageButton* image_button_;

  // Label displaying the zoom percentage.
  views::Label* label_;

  // The WebContents for the page whose zoom has changed.
  content::WebContents* web_contents_;

  // Whether the currently displayed bubble will automatically close.
  bool auto_close_;

  // The immersive mode controller for the BrowserView containing
  // |web_contents_|.
  // Not owned.
  ImmersiveModeController* immersive_mode_controller_;

  DISALLOW_COPY_AND_ASSIGN(ZoomBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ZOOM_BUBBLE_VIEW_H_
