// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_dialog.h"

#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/extensions/extension_dialog_observer.h"
#include "chrome/browser/ui/views/window.h"  // CreateViewsWindow
#include "chrome/common/chrome_notification_types.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "googleurl/src/gurl.h"
#include "ui/views/widget/widget.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/frame/bubble_window.h"
#endif

namespace {

views::Widget* CreateWindow(gfx::NativeWindow parent,
                            views::WidgetDelegate* delegate) {
#if defined(OS_CHROMEOS) && defined(TOOLKIT_USES_GTK)
  // TODO(msw): revert to BubbleWindow for all ChromeOS cases when CL
  // for crbug.com/98322 is landed.
  // On Chrome OS we need to override the style to suppress padding around
  // the borders.
  return chromeos::BubbleWindow::Create(parent,
      STYLE_FLUSH, delegate);
#else
  return browser::CreateViewsWindow(parent, delegate);
#endif
}

}  // namespace

ExtensionDialog::ExtensionDialog(ExtensionHost* host,
                                 ExtensionDialogObserver* observer)
    : window_(NULL),
      extension_host_(host),
      observer_(observer) {
  AddRef();  // Balanced in DeleteDelegate();

  // Listen for the containing view calling window.close();
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE,
                 content::Source<Profile>(host->profile()));
}

ExtensionDialog::~ExtensionDialog() {
}

// static
ExtensionDialog* ExtensionDialog::Show(
    const GURL& url,
    Browser* browser,
    TabContents* tab_contents,
    int width,
    int height,
    const string16& title,
    ExtensionDialogObserver* observer) {
  CHECK(browser);
  ExtensionHost* host = CreateExtensionHost(url, browser);
  if (!host)
    return NULL;
  host->set_associated_tab_contents(tab_contents);

  ExtensionDialog* dialog = new ExtensionDialog(host, observer);
  dialog->set_title(title);
  dialog->InitWindow(browser, width, height);
  // Ensure the DOM JavaScript can respond immediately to keyboard shortcuts.
  host->host_contents()->Focus();
  return dialog;
}

// static
ExtensionHost* ExtensionDialog::CreateExtensionHost(const GURL& url,
                                                    Browser* browser) {
  ExtensionProcessManager* manager =
      browser->profile()->GetExtensionProcessManager();
  DCHECK(manager);
  if (!manager)
    return NULL;
  return manager->CreateDialogHost(url, browser);
}

void ExtensionDialog::InitWindow(Browser* browser, int width, int height) {
  gfx::NativeWindow parent = browser->window()->GetNativeHandle();
  window_ = CreateWindow(parent, this /* views::WidgetDelegate */);

  // Center the window over the browser.
  gfx::Point center = browser->window()->GetBounds().CenterPoint();
  int x = center.x() - width / 2;
  int y = center.y() - height / 2;
  window_->SetBounds(gfx::Rect(x, y, width, height));

  window_->Show();
  window_->Activate();
}

void ExtensionDialog::ObserverDestroyed() {
  observer_ = NULL;
}

void ExtensionDialog::Close() {
  if (!window_)
    return;

  if (observer_)
    observer_->ExtensionDialogIsClosing(this);

  window_->Close();
  window_ = NULL;
}

/////////////////////////////////////////////////////////////////////////////
// views::WidgetDelegate overrides.

bool ExtensionDialog::CanResize() const {
  return false;
}

bool ExtensionDialog::IsModal() const {
  return true;
}

bool ExtensionDialog::ShouldShowWindowTitle() const {
  return !window_title_.empty();
}

string16 ExtensionDialog::GetWindowTitle() const {
  return window_title_;
}

void ExtensionDialog::DeleteDelegate() {
  // The window has finished closing.  Allow ourself to be deleted.
  Release();
}

views::Widget* ExtensionDialog::GetWidget() {
  return extension_host_->view()->GetWidget();
}

const views::Widget* ExtensionDialog::GetWidget() const {
  return extension_host_->view()->GetWidget();
}

views::View* ExtensionDialog::GetContentsView() {
  return extension_host_->view();
}

/////////////////////////////////////////////////////////////////////////////
// content::NotificationObserver overrides.

void ExtensionDialog::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE:
      // If we aren't the host of the popup, then disregard the notification.
      if (content::Details<ExtensionHost>(host()) != details)
        return;
      Close();
      break;
    default:
      NOTREACHED() << L"Received unexpected notification";
      break;
  }
}
