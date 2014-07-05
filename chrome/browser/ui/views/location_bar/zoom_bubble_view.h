// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ZOOM_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ZOOM_BUBBLE_VIEW_H_

#include "base/basictypes.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/extension_icon_image.h"
#include "ui/views/bubble/bubble_delegate.h"
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
class ZoomBubbleView : public views::BubbleDelegateView,
                       public views::ButtonListener,
                       public content::NotificationObserver,
                       public ImmersiveModeController::Observer,
                       public extensions::IconImage::Observer {
 public:
  // Shows the bubble and automatically closes it after a short time period if
  // |auto_close| is true.
  static void ShowBubble(content::WebContents* web_contents,
                         bool auto_close);

  // Closes the showing bubble (if one exists).
  static void CloseBubble();

  // Whether the zoom bubble is currently showing.
  static bool IsShowing();

  // Returns the zoom bubble if the zoom bubble is showing. Returns NULL
  // otherwise.
  static const ZoomBubbleView* GetZoomBubbleForTest();

 private:
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
    scoped_ptr<const extensions::IconImage> icon_image;
  };

  ZoomBubbleView(views::View* anchor_view,
                 content::WebContents* web_contents,
                 bool auto_close,
                 ImmersiveModeController* immersive_mode_controller,
                 FullscreenController* fullscreen_controller);
  virtual ~ZoomBubbleView();

  // If the bubble is not anchored to a view, places the bubble in the top
  // right (left in RTL) of the |screen_bounds| that contain |web_contents_|'s
  // browser window. Because the positioning is based on the size of the
  // bubble, this must be called after the bubble is created.
  void AdjustForFullscreen(const gfx::Rect& screen_bounds);

  // Refreshes the bubble by changing the zoom percentage appropriately and
  // resetting the timer if necessary.
  void Refresh();

  void Close();

  // Sets information about the extension that initiated the zoom change.
  // Calling this method asserts that the extension |extension| did initiate
  // the zoom change.
  void SetExtensionInfo(const extensions::Extension* extension);

  // Starts a timer which will close the bubble if |auto_close_| is true.
  void StartTimerIfNecessary();

  // Stops the auto-close timer.
  void StopTimer();

  // extensions::IconImage::Observer overrides:
  virtual void OnExtensionIconImageChanged(
      extensions::IconImage* /* image */) OVERRIDE;

  // views::View methods.
  virtual void OnMouseEntered(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE;

  // ui::EventHandler method.
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

  // views::ButtonListener method.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // views::BubbleDelegateView method.
  virtual void Init() OVERRIDE;
  virtual void WindowClosing() OVERRIDE;

  // content::NotificationObserver method.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // ImmersiveModeController::Observer methods.
  virtual void OnImmersiveRevealStarted() OVERRIDE;
  virtual void OnImmersiveModeControllerDestroyed() OVERRIDE;

  ZoomBubbleExtensionInfo extension_info_;

  // Singleton instance of the zoom bubble. The zoom bubble can only be shown on
  // the active browser window, so there is no case in which it will be shown
  // twice at the same time.
  static ZoomBubbleView* zoom_bubble_;

  // Timer used to close the bubble when |auto_close_| is true.
  base::OneShotTimer<ZoomBubbleView> timer_;

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

  // Used to register for fullscreen change notifications.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ZoomBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ZOOM_BUBBLE_VIEW_H_
