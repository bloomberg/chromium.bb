// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_popup.h"

#include <vector>

#include "base/bind.h"
#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

#if defined(OS_CHROMEOS) && defined(TOOLKIT_USES_GTK)
#include "chrome/browser/chromeos/legacy_window_manager/wm_ipc.h"
#include "third_party/cros_system_api/window_manager/chromeos_wm_ipc_enums.h"
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

ExtensionPopup::ExtensionPopup(
    Browser* browser,
    ExtensionHost* host,
    const gfx::Rect& relative_to,
    views::BubbleBorder::ArrowLocation arrow_location,
    bool inspect_with_devtools,
    Observer* observer)
    : BrowserBubble(browser,
                    host->view(),
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
                 chrome::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING,
                 content::Source<Profile>(host->profile()));

  // Listen for the containing view calling window.close();
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE,
                 content::Source<Profile>(host->profile()));
}

ExtensionPopup::~ExtensionPopup() {
  // Clear the delegate, because we might trigger UI events during destruction,
  // and we don't want to be called into anymore.
  set_delegate(NULL);
}

void ExtensionPopup::Show(bool activate) {
  if (popup_->IsVisible())
    return;

#if defined(OS_WIN)
  frame_->GetTopLevelWidget()->DisableInactiveRendering();
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
  host()->host_contents()->Focus();
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
  MessageLoop::current()->PostTask(FROM_HERE,
                                   base::Bind(&ExtensionPopup::Close, this));
}


void ExtensionPopup::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING:
      // Once we receive did stop loading, the content will be complete and
      // the width will have been computed.  Now it's safe to show.
      if (extension_host_.get() ==
          content::Details<ExtensionHost>(details).ptr()) {
        Show(true);

        if (inspect_with_devtools_) {
          // Listen for the the devtools window closing.
          registrar_.Add(this, content::NOTIFICATION_DEVTOOLS_WINDOW_CLOSING,
              content::Source<content::BrowserContext>(
                  extension_host_->profile()));
          DevToolsWindow::ToggleDevToolsWindow(
              extension_host_->render_view_host(),
              DEVTOOLS_TOGGLE_ACTION_SHOW_CONSOLE);
        }
      }
      break;
    case chrome::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE:
      // If we aren't the host of the popup, then disregard the notification.
      if (content::Details<ExtensionHost>(host()) != details)
        return;
      Close();

      break;
    case content::NOTIFICATION_DEVTOOLS_WINDOW_CLOSING:
      // Make sure its the devtools window that inspecting our popup.
      if (content::Details<RenderViewHost>(
              extension_host_->render_view_host()) != details)
        return;

      // If the devtools window is closing, we post a task to ourselves to
      // close the popup. This gives the devtools window a chance to finish
      // detaching from the inspected RenderViewHost.
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&ExtensionPopup::Close, this));
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
    views::BubbleBorder::ArrowLocation arrow_location,
    bool inspect_with_devtools,
    Observer* observer) {
  ExtensionProcessManager* manager =
      browser->profile()->GetExtensionProcessManager();
  DCHECK(manager);
  if (!manager)
    return NULL;

  ExtensionHost* host = manager->CreatePopupHost(url, browser);
  ExtensionPopup* popup = new ExtensionPopup(browser, host, relative_to,
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
