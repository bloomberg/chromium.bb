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

using views::Widget;

ExtensionPopup::ExtensionPopup(ExtensionHost* host,
                               Widget* frame,
                               const gfx::Rect& relative_to)
    : BrowserBubble(host->view(), frame, gfx::Point()),
      relative_to_(relative_to),
      extension_host_(host) {
  host->view()->SetContainer(this);
  registrar_.Add(this, NotificationType::EXTENSION_HOST_DID_STOP_LOADING,
                 Source<Profile>(host->profile()));

  // TODO(erikkay) Some of this border code is derived from InfoBubble.
  // We should see if we can unify these classes.

  border_widget_ = Widget::CreatePopupWidget(Widget::Transparent,
                                             Widget::NotAcceptEvents,
                                             Widget::DeleteOnDestroy);
  gfx::NativeView native_window = frame->GetNativeView();
  border_widget_->Init(native_window, bounds());
  border_ = new BubbleBorder;
  border_->set_arrow_location(BubbleBorder::TOP_RIGHT);
  border_view_ = new views::View;
  border_view_->set_background(new BubbleBackground(border_));
  border_view_->set_border(border_);
  border_widget_->SetContentsView(border_view_);
}

ExtensionPopup::~ExtensionPopup() {
  // The widget is set to delete on destroy, so no leak here.
  border_widget_->Close();
}

void ExtensionPopup::Hide() {
  BrowserBubble::Hide();
  border_widget_->Hide();
}

void ExtensionPopup::Show() {
  if (visible())
    return;

  ResizeToView();

  // Show the border first, then the popup overlaid on top.
  border_widget_->Show();
  BrowserBubble::Show(true);
}

void ExtensionPopup::ResizeToView() {
  BrowserBubble::ResizeToView();

  // The rounded corners cut off more of the view than the border insets claim.
  // Since we can't clip the ExtensionView's corners, we need to increase the
  // inset by half the corner radius as well as lying about the size of the
  // contents size to compensate.
  int corner_inset = BubbleBorder::GetCornerRadius() / 2;
  gfx::Size adjusted_size = bounds().size();
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
  MoveTo(origin.x(), origin.y());
}

void ExtensionPopup::Observe(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  if (type == NotificationType::EXTENSION_HOST_DID_STOP_LOADING) {
    // Once we receive did stop loading, the content will be complete and
    // the width will have been computed.  Now it's safe to show.
    if (extension_host_.get() == Details<ExtensionHost>(details).ptr())
      Show();
  } else {
    NOTREACHED() << L"Received unexpected notification";
  }
}

void ExtensionPopup::OnExtensionPreferredSizeChanged(ExtensionView* view) {
  view->SizeToPreferredSize();
  ResizeToView();
}

// static
ExtensionPopup* ExtensionPopup::Show(const GURL& url, Browser* browser,
                                     const gfx::Rect& relative_to) {
  ExtensionProcessManager* manager =
      browser->profile()->GetExtensionProcessManager();
  DCHECK(manager);
  if (!manager)
    return NULL;

  ExtensionHost* host = manager->CreatePopup(url, browser);
  views::Widget* frame = BrowserView::GetBrowserViewForNativeWindow(
      browser->window()->GetNativeHandle())->GetWidget();
  ExtensionPopup* popup = new ExtensionPopup(host, frame, relative_to);

  // If the host had somehow finished loading, then we'd miss the notification
  // and not show.  This seems to happen in single-process mode.
  if (host->did_stop_loading())
    popup->Show();

  return popup;
}
