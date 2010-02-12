// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/extensions/extension_popup.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "third_party/skia/include/core/SkColor.h"
#include "views/widget/root_view.h"
#include "views/window/window.h"

#if defined(OS_LINUX)
#include "chrome/browser/views/tabs/tab_overview_types.h"
#include "views/widget/widget_gtk.h"
#endif

using views::Widget;

// The minimum/maximum dimensions of the popup.
// The minimum is just a little larger than the size of the button itself.
// The maximum is an arbitrary number that should be smaller than most screens.
const int ExtensionPopup::kMinWidth = 25;
const int ExtensionPopup::kMinHeight = 25;
const int ExtensionPopup::kMaxWidth = 800;
const int ExtensionPopup::kMaxHeight = 600;

// The width, in pixels, of the black-border on a popup.
const int kPopupBorderWidth = 1;

const int kPopupBubbleCornerRadius = BubbleBorder::GetCornerRadius() / 2;

ExtensionPopup::ExtensionPopup(ExtensionHost* host,
                               views::Widget* frame,
                               const gfx::Rect& relative_to,
                               BubbleBorder::ArrowLocation arrow_location,
                               bool activate_on_show,
                               PopupChrome chrome)
    : BrowserBubble(host->view(),
                    frame,
                    gfx::Point(),
                    RECTANGLE_CHROME == chrome),  // If no bubble chrome is to
                                                  // be displayed, then enable a
                                                  // drop-shadow on the bubble
                                                  // widget.
      relative_to_(relative_to),
      extension_host_(host),
      activate_on_show_(activate_on_show),
      border_widget_(NULL),
      border_(NULL),
      border_view_(NULL),
      popup_chrome_(chrome),
      anchor_position_(arrow_location) {
  host->view()->SetContainer(this);
  registrar_.Add(this,
                 NotificationType::EXTENSION_HOST_DID_STOP_LOADING,
                 Source<Profile>(host->profile()));

  // TODO(erikkay) Some of this border code is derived from InfoBubble.
  // We should see if we can unify these classes.

  // The bubble chrome requires a separate window, so construct it here.
  if (BUBBLE_CHROME == popup_chrome_) {
    gfx::NativeView native_window = frame->GetNativeView();
#if defined(OS_LINUX)
    border_widget_ = new views::WidgetGtk(views::WidgetGtk::TYPE_WINDOW);
    static_cast<views::WidgetGtk*>(border_widget_)->MakeTransparent();
    static_cast<views::WidgetGtk*>(border_widget_)->make_transient_to_parent();
#else
    border_widget_ = Widget::CreatePopupWidget(Widget::Transparent,
                                               Widget::NotAcceptEvents,
                                               Widget::DeleteOnDestroy);
#endif
    border_widget_->Init(native_window, bounds());
#if defined(OS_LINUX)
    TabOverviewTypes::instance()->SetWindowType(
        border_widget_->GetNativeView(),
        TabOverviewTypes::WINDOW_TYPE_CHROME_INFO_BUBBLE,
        NULL);
#endif

    border_ = new BubbleBorder;
    border_->set_arrow_location(arrow_location);

    border_view_ = new views::View;
    border_view_->set_background(new BubbleBackground(border_));

    border_view_->set_border(border_);
    border_widget_->SetContentsView(border_view_);
    // Ensure that the popup contents are always displayed ontop of the border
    // widget.
    border_widget_->MoveAbove(popup_);
  } else {
    // Otherwise simply set a black-border on the view containing the popup
    // extension view.
    views::Border* border = views::Border::CreateSolidBorder(kPopupBorderWidth,
                                                             SK_ColorBLACK);
    view()->set_border(border);
  }
}

ExtensionPopup::~ExtensionPopup() {
  // The widget is set to delete on destroy, so no leak here.
  if (border_widget_)
    border_widget_->Close();
}

void ExtensionPopup::Hide() {
  BrowserBubble::Hide();
  if (border_widget_)
    border_widget_->Hide();
}

void ExtensionPopup::Show(bool activate) {
  if (visible())
    return;

#if defined(OS_WIN)
  if (frame_->GetWindow())
    frame_->GetWindow()->DisableInactiveRendering();
#endif

  ResizeToView();

  // Show the border first, then the popup overlaid on top.
  if (border_widget_)
    border_widget_->Show();
  BrowserBubble::Show(activate);
}

void ExtensionPopup::ResizeToView() {
  // We'll be sizing ourselves to this size shortly, but wait until we
  // know our position to do it.
  gfx::Size new_size = view()->size();

  gfx::Rect rect = GetOuterBounds(relative_to_, new_size);
  gfx::Point origin = rect.origin();
  views::View::ConvertPointToView(NULL, frame_->GetRootView(), &origin);
  if (border_widget_) {
    // Set the bubble-chrome widget according to the outer bounds of the entire
    // popup.
    border_widget_->SetBounds(rect);

    // Now calculate the inner bounds.  This is a bit more convoluted than
    // it should be because BrowserBubble coordinates are in Browser coordinates
    // while |rect| is in screen coordinates.
    gfx::Insets border_insets;
    border_->GetInsets(&border_insets);

    origin.set_x(origin.x() + border_insets.left() + kPopupBubbleCornerRadius);
    origin.set_y(origin.y() + border_insets.top() + kPopupBubbleCornerRadius);

    SetBounds(origin.x(), origin.y(), new_size.width(), new_size.height());
  } else {
    SetBounds(origin.x(), origin.y(), rect.width(), rect.height());
  }
}

void ExtensionPopup::Observe(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  if (type == NotificationType::EXTENSION_HOST_DID_STOP_LOADING) {
    // Once we receive did stop loading, the content will be complete and
    // the width will have been computed.  Now it's safe to show.
    if (extension_host_.get() == Details<ExtensionHost>(details).ptr())
      Show(activate_on_show_);
  } else {
    NOTREACHED() << L"Received unexpected notification";
  }
}

void ExtensionPopup::OnExtensionPreferredSizeChanged(ExtensionView* view) {
  // Constrain the size to popup min/max.
  gfx::Size sz = view->GetPreferredSize();
  view->SetBounds(view->x(), view->y(),
      std::max(kMinWidth, std::min(kMaxWidth, sz.width())),
      std::max(kMinHeight, std::min(kMaxHeight, sz.height())));

  ResizeToView();
}

gfx::Rect ExtensionPopup::GetOuterBounds(const gfx::Rect& position_relative_to,
                                         const gfx::Size& contents_size) const {
  gfx::Size adjusted_size = contents_size;
  // If the popup has a bubble-chrome, then let the BubbleBorder compute
  // the bounds.
  if (BUBBLE_CHROME == popup_chrome_) {
    // The rounded corners cut off more of the view than the border insets
    // claim. Since we can't clip the ExtensionView's corners, we need to
    // increase the inset by half the corner radius as well as lying about the
    // size of the contents size to compensate.
    adjusted_size.Enlarge(2 * kPopupBubbleCornerRadius,
                          2 * kPopupBubbleCornerRadius);
    return border_->GetBounds(position_relative_to, adjusted_size);
  }

  // Otherwise, enlarge the bounds by the size of the local border.
  gfx::Insets border_insets;
  view()->border()->GetInsets(&border_insets);
  adjusted_size.Enlarge(border_insets.width(), border_insets.height());

  // Position the bounds according to the location of the |anchor_position_|.
  int y;
  if ((anchor_position_ == BubbleBorder::TOP_LEFT) ||
      (anchor_position_ == BubbleBorder::TOP_RIGHT)) {
    y = position_relative_to.bottom();
  } else {
    y = position_relative_to.y() - adjusted_size.height();
  }

  return gfx::Rect(position_relative_to.x(), y, adjusted_size.width(),
                   adjusted_size.height());
}

// static
ExtensionPopup* ExtensionPopup::Show(
    const GURL& url,
    Browser* browser,
    Profile* profile,
    gfx::NativeWindow frame_window,
    const gfx::Rect& relative_to,
    BubbleBorder::ArrowLocation arrow_location,
    bool activate_on_show,
    PopupChrome chrome) {
  DCHECK(profile);
  DCHECK(frame_window);
  ExtensionProcessManager* manager = profile->GetExtensionProcessManager();
  DCHECK(manager);
  if (!manager)
    return NULL;

  // If no Browser instance was given, attempt to look up one matching the given
  // profile.
  if (!browser)
    browser = BrowserList::FindBrowserWithProfile(profile);

  Widget* frame_widget = Widget::GetWidgetFromNativeWindow(frame_window);
  DCHECK(frame_widget);
  if (!frame_widget)
    return NULL;

  ExtensionHost* host = manager->CreatePopup(url, browser);
  ExtensionPopup* popup = new ExtensionPopup(host, frame_widget, relative_to,
                                             arrow_location, activate_on_show,
                                             chrome);

  // If the host had somehow finished loading, then we'd miss the notification
  // and not show.  This seems to happen in single-process mode.
  if (host->did_stop_loading())
    popup->Show(activate_on_show);

  return popup;
}
