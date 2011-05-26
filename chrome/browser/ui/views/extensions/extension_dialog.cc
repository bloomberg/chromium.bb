// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_dialog.h"

#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/bubble/bubble_border.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/extensions/extension.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"
#include "content/common/notification_type.h"
#include "googleurl/src/gurl.h"
#include "views/widget/root_view.h"
#include "views/window/window.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/wm_ipc.h"
#include "third_party/cros/chromeos_wm_ipc_enums.h"
#endif

ExtensionDialog::ExtensionDialog(ExtensionHost* host, views::Widget* frame,
                                 const gfx::Rect& relative_to, int width,
                                 int height, Observer* observer)
    : BrowserBubble(host->view(),
                    frame,
                    relative_to,
                    BubbleBorder::FLOAT),  // Centered over rectangle.
      extension_host_(host),
      width_(width),
      height_(height),
      closing_(false),
      observer_(observer) {
  AddRef();  // Balanced in Close();
  set_delegate(this);
  host->view()->SetContainer(this);

  // Listen for the containing view calling window.close();
  registrar_.Add(this, NotificationType::EXTENSION_HOST_VIEW_SHOULD_CLOSE,
                 Source<Profile>(host->profile()));

  // Use a fixed-size view.
  host->view()->SetBounds(host->view()->x(), host->view()->y(),
                          width, height);
  ResizeToView();
}

ExtensionDialog::~ExtensionDialog() {
}

// static
ExtensionDialog* ExtensionDialog::Show(
    const GURL& url,
    Browser* browser,
    int width,
    int height,
    Observer* observer) {
  CHECK(browser);
  ExtensionProcessManager* manager =
      browser->profile()->GetExtensionProcessManager();
  DCHECK(manager);
  if (!manager)
    return NULL;
  ExtensionHost* host = manager->CreateDialogHost(url, browser);
  views::Widget* frame = BrowserView::GetBrowserViewForNativeWindow(
      browser->window()->GetNativeHandle())->GetWidget();
  gfx::Rect relative_to = browser->window()->GetBounds();
  ExtensionDialog* popup = new ExtensionDialog(host, frame, relative_to, width,
                                               height, observer);
  popup->Show(true);

  return popup;
}

void ExtensionDialog::Close() {
  if (closing_)
    return;
  closing_ = true;
  DetachFromBrowser();

  if (observer_)
    observer_->ExtensionDialogIsClosing(this);

  Release();  // Balanced in ctor.
}

void ExtensionDialog::Show(bool activate) {
  if (popup_->IsVisible())
    return;

#if defined(OS_WIN)
  frame_->GetContainingWindow()->DisableInactiveRendering();
#endif

  BrowserBubble::Show(activate);
}

/////////////////////////////////////////////////////////////////////////////
// BrowserBubble::Delegate methods.

void ExtensionDialog::BubbleBrowserWindowMoved(BrowserBubble* bubble) {
}

void ExtensionDialog::BubbleBrowserWindowClosing(BrowserBubble* bubble) {
  if (!closing_)
    Close();
}

void ExtensionDialog::BubbleGotFocus(BrowserBubble* bubble) {
  // Forward the focus to the renderer.
  host()->render_view_host()->view()->Focus();
}

void ExtensionDialog::BubbleLostFocus(BrowserBubble* bubble,
    bool lost_focus_to_child) {
}

/////////////////////////////////////////////////////////////////////////////
// ExtensionView::Container overrides.

void ExtensionDialog::OnExtensionMouseMove(ExtensionView* view) {
}

void ExtensionDialog::OnExtensionMouseLeave(ExtensionView* view) {
}

void ExtensionDialog::OnExtensionPreferredSizeChanged(ExtensionView* view) {
}

/////////////////////////////////////////////////////////////////////////////
// NotificationObserver overrides.

void ExtensionDialog::Observe(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::EXTENSION_HOST_VIEW_SHOULD_CLOSE:
      // If we aren't the host of the popup, then disregard the notification.
      if (Details<ExtensionHost>(host()) != details)
        return;
      Close();
      break;
    default:
      NOTREACHED() << L"Received unexpected notification";
      break;
  }
}
