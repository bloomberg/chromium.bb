// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_popup.h"

#include <vector>

#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/debugger/devtools_toggle_action.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "views/widget/root_view.h"
#include "views/window/window.h"

#if defined(OS_LINUX)
#include "views/widget/widget_gtk.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/wm_ipc.h"
#include "third_party/cros/chromeos_wm_ipc_enums.h"
#endif

using std::vector;
using views::Widget;

// The minimum/maximum dimensions of the popup.
// The minimum is just a little larger than the size of the button itself.
// The maximum is an arbitrary number that should be smaller than most screens.
const int ExtensionPopup::kMinWidth = 25;
const int ExtensionPopup::kMinHeight = 25;
const int ExtensionPopup::kMaxWidth = 800;
const int ExtensionPopup::kMaxHeight = 600;

namespace {

// The width, in pixels, of the black-border on a popup.
const int kPopupBorderWidth = 1;

const int kPopupBubbleCornerRadius = BubbleBorder::GetCornerRadius() / 2;

}  // namespace

ExtensionPopup::ExtensionPopup(ExtensionHost* host,
                               views::Widget* frame,
                               const gfx::Rect& relative_to,
                               BubbleBorder::ArrowLocation arrow_location,
                               bool inspect_with_devtools,
                               Observer* observer)
    : BrowserBubble(host->view(),
                    frame,
                    gfx::Point()),
      relative_to_(relative_to),
      extension_host_(host),
      inspect_with_devtools_(inspect_with_devtools),
      close_on_lost_focus_(true),
      closing_(false),
      border_widget_(NULL),
      border_(NULL),
      border_view_(NULL),
      observer_(observer) {
  AddRef();  // Balanced in Close();
  set_delegate(this);
  host->view()->SetContainer(this);

  // We wait to show the popup until the contained host finishes loading.
  registrar_.Add(this,
                 NotificationType::EXTENSION_HOST_DID_STOP_LOADING,
                 Source<Profile>(host->profile()));

  // Listen for the containing view calling window.close();
  registrar_.Add(this, NotificationType::EXTENSION_HOST_VIEW_SHOULD_CLOSE,
                 Source<Profile>(host->profile()));

  // TODO(erikkay) Some of this border code is derived from InfoBubble.
  // We should see if we can unify these classes.

  // Keep relative_to_ in frame-relative coordinates to aid in drag
  // positioning.
  gfx::Point origin = relative_to_.origin();
  views::View::ConvertPointToView(NULL, frame_->GetRootView(), &origin);
  relative_to_.set_origin(origin);

  // The bubble chrome requires a separate window, so construct it here.
  gfx::NativeView native_window = frame->GetNativeView();
#if defined(OS_LINUX)
  border_widget_ = new views::WidgetGtk(views::WidgetGtk::TYPE_WINDOW);
  static_cast<views::WidgetGtk*>(border_widget_)->MakeTransparent();
  static_cast<views::WidgetGtk*>(border_widget_)->make_transient_to_parent();
#else
  border_widget_ = Widget::CreatePopupWidget(Widget::Transparent,
                                             Widget::NotAcceptEvents,
                                             Widget::DeleteOnDestroy,
                                             Widget::MirrorOriginInRTL);
#endif
  border_widget_->Init(native_window, bounds());
#if defined(OS_CHROMEOS)
  {
    vector<int> params;
    params.push_back(0);  // don't show while screen is locked
    chromeos::WmIpc::instance()->SetWindowType(
        border_widget_->GetNativeView(),
        chromeos::WM_IPC_WINDOW_CHROME_INFO_BUBBLE,
        &params);
  }
#endif
  border_ = new BubbleBorder(arrow_location);
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

void ExtensionPopup::BubbleBrowserWindowMoved(BrowserBubble* bubble) {
  ResizeToView();
}

void ExtensionPopup::BubbleBrowserWindowClosing(BrowserBubble* bubble) {
  if (!closing_)
    Close();
}

void ExtensionPopup::BubbleGotFocus(BrowserBubble* bubble) {
  // Forward the focus to the renderer.
  host()->render_view_host()->view()->Focus();
}

void ExtensionPopup::BubbleLostFocus(BrowserBubble* bubble,
    bool lost_focus_to_child) {
  if (closing_ ||                // We are already closing.
      inspect_with_devtools_ ||  // The popup is being inspected.
      !close_on_lost_focus_ ||   // Our client is handling focus listening.
      lost_focus_to_child)       // A child of this view got focus.
    return;

  // When we do close on BubbleLostFocus, we do it in the next event loop
  // because a subsequent event in this loop may also want to close this popup
  // and if so, we want to allow that. Example: Clicking the same browser
  // action button that opened the popup. If we closed immediately, the
  // browser action container would fail to discover that the same button
  // was pressed.
  MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(this,
      &ExtensionPopup::Close));
}


void ExtensionPopup::Observe(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::EXTENSION_HOST_DID_STOP_LOADING:
      // Once we receive did stop loading, the content will be complete and
      // the width will have been computed.  Now it's safe to show.
      if (extension_host_.get() == Details<ExtensionHost>(details).ptr()) {
        Show(true);

        if (inspect_with_devtools_) {
          // Listen for the the devtools window closing.
          registrar_.Add(this, NotificationType::DEVTOOLS_WINDOW_CLOSING,
              Source<Profile>(extension_host_->profile()));
          DevToolsManager::GetInstance()->ToggleDevToolsWindow(
              extension_host_->render_view_host(),
              DEVTOOLS_TOGGLE_ACTION_SHOW_CONSOLE);
        }
      }
      break;
    case NotificationType::EXTENSION_HOST_VIEW_SHOULD_CLOSE:
      // If we aren't the host of the popup, then disregard the notification.
      if (Details<ExtensionHost>(host()) != details)
        return;
      Close();

      break;
    case NotificationType::DEVTOOLS_WINDOW_CLOSING:
      // Make sure its the devtools window that inspecting our popup.
      if (Details<RenderViewHost>(extension_host_->render_view_host()) !=
          details)
        return;

      // If the devtools window is closing, we post a task to ourselves to
      // close the popup. This gives the devtools window a chance to finish
      // detaching from the inspected RenderViewHost.
      MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(this,
          &ExtensionPopup::Close));

      break;
    default:
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
    const GURL& url,
    Browser* browser,
    const gfx::Rect& relative_to,
    BubbleBorder::ArrowLocation arrow_location,
    bool inspect_with_devtools,
    Observer* observer) {
  ExtensionProcessManager* manager =
      browser->profile()->GetExtensionProcessManager();
  DCHECK(manager);
  if (!manager)
    return NULL;

  ExtensionHost* host = manager->CreatePopup(url, browser);
  views::Widget* frame = BrowserView::GetBrowserViewForNativeWindow(
      browser->window()->GetNativeHandle())->GetWidget();
  ExtensionPopup* popup = new ExtensionPopup(host, frame, relative_to,
                                             arrow_location,
                                             inspect_with_devtools, observer);

  // If the host had somehow finished loading, then we'd miss the notification
  // and not show.  This seems to happen in single-process mode.
  if (host->did_stop_loading())
    popup->Show(true);

  return popup;
}

void ExtensionPopup::Close() {
  if (closing_)
    return;
  closing_ = true;
  DetachFromBrowser();

  if (observer_)
    observer_->ExtensionPopupIsClosing(this);

  Release();  // Balanced in ctor.
}
