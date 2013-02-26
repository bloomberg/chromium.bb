// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_dialog.h"

#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/base_window.h"
#include "chrome/browser/ui/views/extensions/extension_dialog_observer.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/screen.h"
#include "ui/views/background.h"
#include "ui/views/widget/widget.h"

#if defined(USE_ASH)
#include "ash/shell.h"
#include "ui/aura/root_window.h"
#endif

using content::WebContents;

ExtensionDialog::ExtensionDialog(extensions::ExtensionHost* host,
                                 ExtensionDialogObserver* observer)
    : window_(NULL),
      extension_host_(host),
      observer_(observer) {
  AddRef();  // Balanced in DeleteDelegate();

  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING,
                 content::Source<Profile>(host->profile()));
  // Listen for the containing view calling window.close();
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE,
                 content::Source<Profile>(host->profile()));
  // Listen for a crash or other termination of the extension process.
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_PROCESS_TERMINATED,
                 content::Source<Profile>(host->profile()));
}

ExtensionDialog::~ExtensionDialog() {
}

// static
ExtensionDialog* ExtensionDialog::Show(
    const GURL& url,
    BaseWindow* base_window,
    Profile* profile,
    WebContents* web_contents,
    int width,
    int height,
    const string16& title,
    ExtensionDialogObserver* observer) {
  extensions::ExtensionHost* host = CreateExtensionHost(url, profile);
  if (!host)
    return NULL;
  host->SetAssociatedWebContents(web_contents);

  return ExtensionDialog::ShowInternal(url, base_window, host, width, height,
                                       false, title, observer);
}

#if defined(USE_AURA)
// static
ExtensionDialog* ExtensionDialog::ShowFullscreen(
    const GURL& url,
    Profile* profile,
    const string16& title,
    ExtensionDialogObserver* observer) {
  extensions::ExtensionHost* host = CreateExtensionHost(url, profile);
  if (!host)
    return NULL;

  return ExtensionDialog::ShowInternal(url, NULL, host, 0, 0,
                                       true, title, observer);
}
#endif

// static
ExtensionDialog* ExtensionDialog::ShowInternal(
    const GURL& url,
    BaseWindow* base_window,
    extensions::ExtensionHost* host,
    int width,
    int height,
    bool fullscreen,
    const string16& title,
    ExtensionDialogObserver* observer) {
  CHECK(fullscreen || base_window);
  ExtensionDialog* dialog = new ExtensionDialog(host, observer);
  dialog->set_title(title);

  if (fullscreen)
    dialog->InitWindowFullscreen();
  else
    dialog->InitWindow(base_window, width, height);

  // Show a white background while the extension loads.  This is prettier than
  // flashing a black unfilled window frame.
  host->view()->set_background(
      views::Background::CreateSolidBackground(0xFF, 0xFF, 0xFF));
  host->view()->SetVisible(true);

  // Ensure the DOM JavaScript can respond immediately to keyboard shortcuts.
  host->host_contents()->GetView()->Focus();
  return dialog;
}

// static
extensions::ExtensionHost* ExtensionDialog::CreateExtensionHost(
    const GURL& url,
    Profile* profile) {
  DCHECK(profile);
  ExtensionProcessManager* manager =
      extensions::ExtensionSystem::Get(profile)->process_manager();

  DCHECK(manager);
  if (!manager)
    return NULL;
  return manager->CreateDialogHost(url);
}

#if defined(USE_ASH)
void ExtensionDialog::InitWindowFullscreen() {
  aura::RootWindow* root_window = ash::Shell::GetPrimaryRootWindow();
  gfx::Rect screen_rect =
      ash::Shell::GetScreen()->GetDisplayNearestWindow(root_window).bounds();

  // We want to be the fullscreen topmost child of the root window.
  window_ = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = this;
  params.parent = root_window;
  window_->Init(params);
  window_->StackAtTop();
  window_->SetBounds(screen_rect);
  window_->Show();

  // TODO(jamescook): Remove redundant call to Activate()?
  window_->Activate();
}
#else
void ExtensionDialog::InitWindowFullscreen() {
  NOTIMPLEMENTED();
}
#endif


void ExtensionDialog::InitWindow(BaseWindow* base_window,
                                 int width,
                                 int height) {
  gfx::NativeWindow parent = base_window->GetNativeWindow();
  window_ = views::Widget::CreateWindowWithParent(this, parent);

  // Center the window over the browser.
  gfx::Point center = base_window->GetBounds().CenterPoint();
  int x = center.x() - width / 2;
  int y = center.y() - height / 2;
  // Ensure the top left and top right of the window are on screen, with
  // priority given to the top left.
  gfx::Rect screen_rect = gfx::Screen::GetScreenFor(parent)->
      GetDisplayNearestPoint(center).bounds();
  gfx::Rect bounds_rect = gfx::Rect(x, y, width, height);
  bounds_rect.AdjustToFit(screen_rect);
  window_->SetBounds(bounds_rect);

  window_->Show();
  // TODO(jamescook): Remove redundant call to Activate()?
  window_->Activate();
}

void ExtensionDialog::ObserverDestroyed() {
  observer_ = NULL;
}

void ExtensionDialog::Close() {
  if (!window_)
    return;

  window_->Close();
  window_ = NULL;
}

void ExtensionDialog::MaybeFocusRenderView() {
  views::FocusManager* focus_manager = GetWidget()->GetFocusManager();
  DCHECK(focus_manager != NULL);

  // Already there's a focused view, so no need to switch the focus.
  if (focus_manager->GetFocusedView())
    return;

  content::RenderWidgetHostView* view = host()->render_view_host()->GetView();
  if (!view)
    return;

  // TODO(oshima): Views + aura doesn't seem to update the views focus
  // manager when an aura window gets focus. This is a workaround for
  // this issue. Fix it this and remove this workaround.
  // See bug.com/127222.
  focus_manager->SetFocusedView(GetContentsView());
  view->Focus();
}

/////////////////////////////////////////////////////////////////////////////
// views::WidgetDelegate overrides.

bool ExtensionDialog::CanResize() const {
  // Can resize only if minimum contents size set.
  return extension_host_->view()->GetPreferredSize() != gfx::Size();
}

void ExtensionDialog::SetMinimumContentsSize(int width, int height) {
  extension_host_->view()->SetPreferredSize(gfx::Size(width, height));
}

ui::ModalType ExtensionDialog::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

bool ExtensionDialog::ShouldShowWindowTitle() const {
  return !window_title_.empty();
}

string16 ExtensionDialog::GetWindowTitle() const {
  return window_title_;
}

void ExtensionDialog::WindowClosing() {
  if (observer_)
    observer_->ExtensionDialogClosing(this);
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
    case chrome::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING:
      // Avoid potential overdraw by removing the temporary background after
      // the extension finishes loading.
      extension_host_->view()->set_background(NULL);
      // The render view is created during the LoadURL(), so we should
      // set the focus to the view if nobody else takes the focus.
      if (content::Details<extensions::ExtensionHost>(host()) == details)
        MaybeFocusRenderView();
      break;
    case chrome::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE:
      // If we aren't the host of the popup, then disregard the notification.
      if (content::Details<extensions::ExtensionHost>(host()) != details)
        return;
      Close();
      break;
    case chrome::NOTIFICATION_EXTENSION_PROCESS_TERMINATED:
      if (content::Details<extensions::ExtensionHost>(host()) != details)
        return;
      if (observer_)
        observer_->ExtensionTerminated(this);
      break;
    default:
      NOTREACHED() << L"Received unexpected notification";
      break;
  }
}
