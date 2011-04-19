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
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/extensions/extension.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"
#include "content/common/notification_type.h"
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

ExtensionPopup::ExtensionPopup(ExtensionHost* host,
                               views::Widget* frame,
                               const gfx::Rect& relative_to,
                               BubbleBorder::ArrowLocation arrow_location,
                               bool inspect_with_devtools,
                               Observer* observer)
    : BrowserBubble(host->view(),
                    frame,
                    relative_to,
                    arrow_location),
      relative_to_(relative_to),
      extension_host_(host),
      inspect_with_devtools_(inspect_with_devtools),
      close_on_lost_focus_(true),
      closing_(false),
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
}

ExtensionPopup::~ExtensionPopup() {
}

void ExtensionPopup::Show(bool activate) {
  if (visible())
    return;

#if defined(OS_WIN)
  frame_->GetWindow()->DisableInactiveRendering();
#endif

  ResizeToView();
  BrowserBubble::Show(activate);
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
