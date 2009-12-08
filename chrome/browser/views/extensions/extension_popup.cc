// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/extensions/extension_popup.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "views/widget/root_view.h"
#include "views/window/window.h"

using views::Widget;

// The minimum/maximum dimensions of the popup.
// The minimum is just a little larger than the size of the button itself.
// The maximum is an arbitrary number that should be smaller than most screens.
const int ExtensionPopup::kMinWidth = 25;
const int ExtensionPopup::kMinHeight = 25;
const int ExtensionPopup::kMaxWidth = 800;
const int ExtensionPopup::kMaxHeight = 600;

ExtensionPopup::ExtensionPopup(ExtensionHost* host,
                               Widget* frame,
                               const gfx::Rect& relative_to,
                               BubbleBorder::ArrowLocation arrow_location,
                               bool activate_on_show)
    : BrowserBubble(host->view(),
                    frame,
                    gfx::Point()),
      relative_to_(relative_to),
      extension_host_(host),
      activate_on_show_(activate_on_show) {
  host->view()->SetContainer(this);
  registrar_.Add(this,
                 NotificationType::EXTENSION_HOST_DID_STOP_LOADING,
                 Source<Profile>(host->profile()));

  // TODO(erikkay) Some of this border code is derived from InfoBubble.
  // We should see if we can unify these classes.

  border_widget_ = Widget::CreatePopupWidget(Widget::Transparent,
                                             Widget::NotAcceptEvents,
                                             Widget::DeleteOnDestroy);
  gfx::NativeView native_window = frame->GetNativeView();
  border_widget_->Init(native_window, bounds());

  border_ = new BubbleBorder;
  border_->set_arrow_location(arrow_location);

  border_view_ = new views::View;
  border_view_->set_background(new BubbleBackground(border_));
  border_view_->set_border(border_);
  border_widget_->SetContentsView(border_view_);

  // Ensure that the popup contents are always displayed ontop of the border
  // widget.
  border_widget_->MoveAbove(popup_);
}

ExtensionPopup::~ExtensionPopup() {
  // The widget is set to delete on destroy, so no leak here.
  border_widget_->Close();
}

void ExtensionPopup::Hide() {
  BrowserBubble::Hide();
  border_widget_->Hide();
}

void ExtensionPopup::Show(bool activate) {
  if (visible())
    return;

#if defined(OS_WIN)
  frame_->GetWindow()->DisableInactiveRendering();
#endif

  ResizeToView();

  // Show the border first, then the popup overlaid on top.
  border_widget_->Show();
  BrowserBubble::Show(activate);
}

void ExtensionPopup::ResizeToView() {
  // We'll be sizing ourselves to this size shortly, but wait until we
  // know our position to do it.
  gfx::Size new_size = view()->size();

  // The rounded corners cut off more of the view than the border insets claim.
  // Since we can't clip the ExtensionView's corners, we need to increase the
  // inset by half the corner radius as well as lying about the size of the
  // contents size to compensate.
  int corner_inset = BubbleBorder::GetCornerRadius() / 2;
  gfx::Size adjusted_size = new_size;
  adjusted_size.Enlarge(2 * corner_inset, 2 * corner_inset);
  gfx::Rect rect = border_->GetBounds(relative_to_, adjusted_size);
  border_widget_->SetBounds(rect);

  // Now calculate the inner bounds.  This is a bit more convoluted than
  // it should be because BrowserBubble coordinates are in Browser coordinates
  // while |rect| is in screen coordinates.
  gfx::Insets border_insets;
  border_->GetInsets(&border_insets);
  gfx::Point origin = rect.origin();
  views::View::ConvertPointToView(NULL, frame_->GetRootView(), &origin);
  origin.set_x(origin.x() + border_insets.left() + corner_inset);
  origin.set_y(origin.y() + border_insets.top() + corner_inset);

  SetBounds(origin.x(), origin.y(), new_size.width(), new_size.height());
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

// static
ExtensionPopup* ExtensionPopup::Show(
    const GURL& url, Browser* browser,
    const gfx::Rect& relative_to,
    BubbleBorder::ArrowLocation arrow_location,
    bool activate_on_show) {
  ExtensionProcessManager* manager =
      browser->profile()->GetExtensionProcessManager();
  DCHECK(manager);
  if (!manager)
    return NULL;

  ExtensionHost* host = manager->CreatePopup(url, browser);
  views::Widget* frame = BrowserView::GetBrowserViewForNativeWindow(
      browser->window()->GetNativeHandle())->GetWidget();
  ExtensionPopup* popup = new ExtensionPopup(host, frame, relative_to,
                                             arrow_location, activate_on_show);

  // If the host had somehow finished loading, then we'd miss the notification
  // and not show.  This seems to happen in single-process mode.
  if (host->did_stop_loading())
    popup->Show(activate_on_show);

  return popup;
}
