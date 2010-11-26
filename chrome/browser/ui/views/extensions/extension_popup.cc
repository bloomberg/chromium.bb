// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/extensions/extension_popup.h"

#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/debugger/devtools_toggle_action.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/window_sizer.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "third_party/skia/include/core/SkColor.h"
#include "views/widget/root_view.h"
#include "views/window/window.h"


#if defined(OS_LINUX)
#include "views/widget/widget_gtk.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/wm_ipc.h"
#include "cros/chromeos_wm_ipc_enums.h"
#endif

using views::Widget;

// The minimum, and default maximum dimensions of the popup.
// The minimum is just a little larger than the size of the button itself.
// The default maximum is an arbitrary number that should be smaller than most
// screens.
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
                               bool activate_on_show,
                               bool inspect_with_devtools,
                               PopupChrome chrome,
                               Observer* observer)
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
      inspect_with_devtools_(inspect_with_devtools),
      close_on_lost_focus_(true),
      closing_(false),
      border_widget_(NULL),
      border_(NULL),
      border_view_(NULL),
      popup_chrome_(chrome),
      max_size_(kMaxWidth, kMaxHeight),
      observer_(observer),
      anchor_position_(arrow_location),
      instance_lifetime_(new InternalRefCounter()){
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
  if (BUBBLE_CHROME == popup_chrome_) {
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
    chromeos::WmIpc::instance()->SetWindowType(
        border_widget_->GetNativeView(),
        chromeos::WM_IPC_WINDOW_CHROME_INFO_BUBBLE,
        NULL);
#endif
    border_ = new BubbleBorder(arrow_location);
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

void ExtensionPopup::SetArrowPosition(
    BubbleBorder::ArrowLocation arrow_location) {
  DCHECK_NE(BubbleBorder::NONE, arrow_location) <<
    "Extension popups must be positioned relative to an arrow.";

  anchor_position_ = arrow_location;
  if (border_)
    border_->set_arrow_location(anchor_position_);
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
  if (observer_)
    observer_->ExtensionPopupResized(this);

  gfx::Rect rect = GetOuterBounds();

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

    gfx::Size new_size = view()->size();
    SetBounds(origin.x(), origin.y(), new_size.width(), new_size.height());
  } else {
    SetBounds(origin.x(), origin.y(), rect.width(), rect.height());
  }
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
        Show(activate_on_show_);

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

  // Enforce that the popup never resizes to larger than the working monitor
  // bounds.
  scoped_ptr<WindowSizer::MonitorInfoProvider> monitor_provider(
      WindowSizer::CreateDefaultMonitorInfoProvider());
  gfx::Rect monitor_bounds(
      monitor_provider->GetMonitorWorkAreaMatching(relative_to_));

  int max_width = std::min(max_size_.width(), monitor_bounds.width());
  int max_height = std::min(max_size_.height(), monitor_bounds.height());
  view->SetBounds(view->x(), view->y(),
      std::max(kMinWidth, std::min(max_width, sz.width())),
      std::max(kMinHeight, std::min(max_height, sz.height())));

  // If popup_chrome_ == RECTANGLE_CHROME, the border is drawn in the client
  // area of the ExtensionView, rather than in a window which sits behind it.
  // In this case, the actual size of the view must be enlarged so that the
  // web contents portion of the view gets its full PreferredSize area.
  if (view->border()) {
    gfx::Insets border_insets;
    view->border()->GetInsets(&border_insets);

    gfx::Rect bounds(view->bounds());
    gfx::Size size(bounds.size());
    size.Enlarge(border_insets.width(), border_insets.height());
    view->SetBounds(bounds.x(), bounds.y(), size.width(), size.height());
  }

  ResizeToView();
}

gfx::Rect ExtensionPopup::GetOuterBounds() const {
  gfx::Rect relative_rect = relative_to_;
  gfx::Point origin = relative_rect.origin();
  views::View::ConvertPointToScreen(frame_->GetRootView(), &origin);
  relative_rect.set_origin(origin);

  gfx::Size contents_size = view()->size();

  // If the popup has a bubble-chrome, then let the BubbleBorder compute
  // the bounds.
  if (BUBBLE_CHROME == popup_chrome_) {
    // The rounded corners cut off more of the view than the border insets
    // claim. Since we can't clip the ExtensionView's corners, we need to
    // increase the inset by half the corner radius as well as lying about the
    // size of the contents size to compensate.
    contents_size.Enlarge(2 * kPopupBubbleCornerRadius,
                          2 * kPopupBubbleCornerRadius);
    return border_->GetBounds(relative_rect, contents_size);
  }

  // Position the bounds according to the location of the |anchor_position_|.
  int y;
  if (BubbleBorder::is_arrow_on_top(anchor_position_))
    y = relative_rect.bottom();
  else
    y = relative_rect.y() - contents_size.height();

  int x;
  if (BubbleBorder::is_arrow_on_left(anchor_position_))
    x = relative_rect.x();
  else
    // Note that if the arrow is on the right, that the x position of the popup
    // is assigned so that the rightmost edge of the popup is aligned with the
    // rightmost edge of the relative region.
    x = relative_rect.right() - contents_size.width();

  return gfx::Rect(x, y, contents_size.width(), contents_size.height());
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
    bool inspect_with_devtools,
    PopupChrome chrome,
    Observer* observer) {
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
  if (observer)
    observer->ExtensionHostCreated(host);

  ExtensionPopup* popup = new ExtensionPopup(host, frame_widget, relative_to,
                                             arrow_location, activate_on_show,
                                             inspect_with_devtools, chrome,
                                             observer);

  if (observer)
    observer->ExtensionPopupCreated(popup);

  // If the host had somehow finished loading, then we'd miss the notification
  // and not show.  This seems to happen in single-process mode.
  if (host->did_stop_loading())
    popup->Show(activate_on_show);

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

void ExtensionPopup::Release() {
  bool final_release = instance_lifetime_->HasOneRef();
  instance_lifetime_->Release();
  if (final_release) {
    DCHECK(closing_) << "ExtensionPopup to be destroyed before being closed.";
    ExtensionPopup::Observer* observer = observer_;
    delete this;

    // |this| is passed only as a 'cookie'. The observer API explicitly takes a
    // void* argument to emphasize this.
    if (observer)
      observer->ExtensionPopupClosed(this);
  }
}
