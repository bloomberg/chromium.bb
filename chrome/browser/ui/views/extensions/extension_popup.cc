// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_popup.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/window.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "ui/views/layout/fill_layout.h"

using content::WebContents;

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
    views::View* anchor_view,
    views::BubbleBorder::ArrowLocation arrow_location,
    bool inspect_with_devtools)
    : BubbleDelegateView(anchor_view, arrow_location),
      extension_host_(host),
      inspect_with_devtools_(inspect_with_devtools) {
  // Adjust the margin so that contents fit better.
  set_margin(views::BubbleBorder::GetCornerRadius() / 2);
  SetLayoutManager(new views::FillLayout());
  AddChildView(host->view());
  host->view()->SetContainer(this);
#if defined(OS_WIN) && !defined(USE_AURA)
  // Use OnNativeFocusChange to check for child window activation on deactivate.
  set_close_on_deactivate(false);
#else
  set_close_on_deactivate(!inspect_with_devtools);
#endif

  // Wait to show the popup until the contained host finishes loading.
  registrar_.Add(this, content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
                 content::Source<WebContents>(host->host_contents()));

  // Listen for the containing view calling window.close();
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE,
                 content::Source<Profile>(host->profile()));

  views::WidgetFocusManager::GetInstance()->AddFocusChangeListener(this);
}

ExtensionPopup::~ExtensionPopup() {
  views::WidgetFocusManager::GetInstance()->RemoveFocusChangeListener(this);
}

void ExtensionPopup::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME:
      DCHECK(content::Source<WebContents>(host()->host_contents()) == source);
      // Show when the content finishes loading and its width is computed.
      Show();
      // Focus on the host contents when the bubble is first shown.
      host()->host_contents()->Focus();
      if (inspect_with_devtools_) {
        // Listen for the the devtools window closing.
        registrar_.Add(this, content::NOTIFICATION_DEVTOOLS_WINDOW_CLOSING,
            content::Source<content::BrowserContext>(host()->profile()));
        DevToolsWindow::ToggleDevToolsWindow(host()->render_view_host(),
            DEVTOOLS_TOGGLE_ACTION_SHOW_CONSOLE);
      }
      break;
    case chrome::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE:
      // If we aren't the host of the popup, then disregard the notification.
      if (content::Details<ExtensionHost>(host()) == details)
        GetWidget()->Close();
      break;
    case content::NOTIFICATION_DEVTOOLS_WINDOW_CLOSING:
      // Make sure its the devtools window that inspecting our popup.
      // Widget::Close posts a task, which should give the devtools window a
      // chance to finish detaching from the inspected RenderViewHost.
      if (content::Details<RenderViewHost>(
              host()->render_view_host()) == details)
        GetWidget()->Close();
      break;
    default:
      NOTREACHED() << L"Received unexpected notification";
  }
}

void ExtensionPopup::OnExtensionPreferredSizeChanged(ExtensionView* view) {
  SizeToContents();
}

gfx::Size ExtensionPopup::GetPreferredSize() {
  // Constrain the size to popup min/max.
  gfx::Size sz = views::View::GetPreferredSize();
  sz.set_width(std::max(kMinWidth, std::min(kMaxWidth, sz.width())));
  sz.set_height(std::max(kMinHeight, std::min(kMaxHeight, sz.height())));
  return sz;
}

void ExtensionPopup::OnNativeFocusChange(gfx::NativeView focused_before,
                                         gfx::NativeView focused_now) {
  // TODO(msw): Implement something equivalent for Aura. See crbug.com/106958
#if defined(OS_WIN) && !defined(USE_AURA)
  // Don't close if a child of this window is activated (only needed on Win).
  // ExtensionPopups can create Javascipt dialogs; see crbug.com/106723.
  gfx::NativeView this_window = GetWidget()->GetNativeView();
  if (!inspect_with_devtools_ && focused_before == this_window) {
    DCHECK_NE(focused_now, this_window);
    if (::GetWindow(focused_now, GW_OWNER) == this_window)
      return;
    gfx::NativeView focused_parent = focused_now;
    while (focused_parent = ::GetParent(focused_parent)) {
      if (this_window == focused_parent)
        return;
    }
    GetWidget()->Close();
  }
#endif
}

// static
ExtensionPopup* ExtensionPopup::ShowPopup(
    const GURL& url,
    Browser* browser,
    views::View* anchor_view,
    views::BubbleBorder::ArrowLocation arrow_location,
    bool inspect_with_devtools) {
  ExtensionProcessManager* manager =
      browser->profile()->GetExtensionProcessManager();
  ExtensionHost* host = manager->CreatePopupHost(url, browser);
  ExtensionPopup* popup = new ExtensionPopup(browser, host, anchor_view,
      arrow_location, inspect_with_devtools);
  browser::CreateViewsBubble(popup);

  // If the host had somehow finished loading, then we'd miss the notification
  // and not show.  This seems to happen in single-process mode.
  if (host->did_stop_loading()) {
    popup->Show();
    // Focus on the host contents when the bubble is first shown.
    host->host_contents()->Focus();
  }

  return popup;
}
