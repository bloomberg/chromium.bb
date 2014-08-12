// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/constrained_window_views.h"

#include <algorithm>

#include "chrome/browser/guest_view/web_view/web_view_guest.h"
#include "chrome/browser/ui/browser_finder.h"
#include "components/web_modal/popup_manager.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "ui/views/border.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_delegate.h"

using web_modal::ModalDialogHost;
using web_modal::ModalDialogHostObserver;

namespace {
// The name of a key to store on the window handle to associate
// BrowserModalDialogHostObserverViews with the Widget.
const char* const kBrowserModalDialogHostObserverViewsKey =
    "__BROWSER_MODAL_DIALOG_HOST_OBSERVER_VIEWS__";

// Applies positioning changes from the ModalDialogHost to the Widget.
class BrowserModalDialogHostObserverViews
    : public views::WidgetObserver,
      public ModalDialogHostObserver {
 public:
  BrowserModalDialogHostObserverViews(ModalDialogHost* host,
                                      views::Widget* target_widget,
                                      const char *const native_window_property)
      : host_(host),
        target_widget_(target_widget),
        native_window_property_(native_window_property) {
    DCHECK(host_);
    DCHECK(target_widget_);
    host_->AddObserver(this);
    target_widget_->AddObserver(this);
  }

  virtual ~BrowserModalDialogHostObserverViews() {
    if (host_)
      host_->RemoveObserver(this);
    target_widget_->RemoveObserver(this);
    target_widget_->SetNativeWindowProperty(native_window_property_, NULL);
  }

  // WidgetObserver overrides
  virtual void OnWidgetClosing(views::Widget* widget) OVERRIDE {
    delete this;
  }

  // WebContentsModalDialogHostObserver overrides
  virtual void OnPositionRequiresUpdate() OVERRIDE {
    UpdateBrowserModalDialogPosition(target_widget_, host_);
  }

  virtual void OnHostDestroying() OVERRIDE {
    host_->RemoveObserver(this);
    host_ = NULL;
  }

 private:
  ModalDialogHost* host_;
  views::Widget* target_widget_;
  const char* const native_window_property_;

  DISALLOW_COPY_AND_ASSIGN(BrowserModalDialogHostObserverViews);
};

void UpdateModalDialogPosition(views::Widget* widget,
                               web_modal::ModalDialogHost* dialog_host,
                               const gfx::Size& size) {
  // Do not forcibly update the dialog widget position if it is being dragged.
  if (widget->HasCapture())
    return;

  gfx::Point position = dialog_host->GetDialogPosition(size);
  views::Border* border = widget->non_client_view()->frame_view()->border();
  // Border may be null during widget initialization.
  if (border) {
    // Align the first row of pixels inside the border. This is the apparent
    // top of the dialog.
    position.set_y(position.y() - border->GetInsets().top());
  }

  if (widget->is_top_level()) {
    position +=
        views::Widget::GetWidgetForNativeView(dialog_host->GetHostView())->
            GetClientAreaBoundsInScreen().OffsetFromOrigin();
  }

  widget->SetBounds(gfx::Rect(position, size));
}

}  // namespace

void UpdateWebContentsModalDialogPosition(
    views::Widget* widget,
    web_modal::WebContentsModalDialogHost* dialog_host) {
  gfx::Size size = widget->GetRootView()->GetPreferredSize();
  gfx::Size max_size = dialog_host->GetMaximumDialogSize();
  // Enlarge the max size by the top border, as the dialog will be shifted
  // outside the area specified by the dialog host by this amount later.
  views::Border* border =
      widget->non_client_view()->frame_view()->border();
  // Border may be null during widget initialization.
  if (border)
    max_size.Enlarge(0, border->GetInsets().top());
  size.SetToMin(max_size);
  UpdateModalDialogPosition(widget, dialog_host, size);
}

void UpdateBrowserModalDialogPosition(views::Widget* widget,
                                      web_modal::ModalDialogHost* dialog_host) {
  UpdateModalDialogPosition(widget, dialog_host,
                            widget->GetRootView()->GetPreferredSize());
}

views::Widget* ShowWebModalDialogViews(
    views::WidgetDelegate* dialog,
    content::WebContents* initiator_web_contents) {
  extensions::WebViewGuest* web_view_guest =
      extensions::WebViewGuest::FromWebContents(initiator_web_contents);
  // For embedded WebContents, use the embedder's WebContents for constrained
  // window.
  content::WebContents* web_contents =
      web_view_guest && web_view_guest->embedder_web_contents() ?
          web_view_guest->embedder_web_contents() : initiator_web_contents;
  views::Widget* widget = CreateWebModalDialogViews(dialog, web_contents);
  web_modal::PopupManager* popup_manager =
      web_modal::PopupManager::FromWebContents(web_contents);
  popup_manager->ShowModalDialog(widget->GetNativeWindow(), web_contents);
  return widget;
}

views::Widget* CreateWebModalDialogViews(views::WidgetDelegate* dialog,
                                         content::WebContents* web_contents) {
  DCHECK_EQ(ui::MODAL_TYPE_CHILD, dialog->GetModalType());
  web_modal::PopupManager* popup_manager =
      web_modal::PopupManager::FromWebContents(web_contents);
  const gfx::NativeWindow parent = popup_manager->GetHostView();
  return views::DialogDelegate::CreateDialogWidget(dialog, NULL, parent);
}

// TODO(gbillock): Replace this with PopupManager calls.
views::Widget* CreateBrowserModalDialogViews(views::DialogDelegate* dialog,
                                             gfx::NativeWindow parent) {
  views::Widget* widget =
      views::DialogDelegate::CreateDialogWidget(dialog, NULL, parent);
  if (!dialog->UseNewStyleForThisDialog())
    return widget;

  // Get the browser dialog management and hosting components from |parent|.
  Browser* browser = chrome::FindBrowserWithWindow(parent);
  if (browser) {
    ChromeWebModalDialogManagerDelegate* manager = browser;
    ModalDialogHost* host = manager->GetWebContentsModalDialogHost();
    DCHECK_EQ(parent, host->GetHostView());
    ModalDialogHostObserver* dialog_host_observer =
        new BrowserModalDialogHostObserverViews(
            host, widget, kBrowserModalDialogHostObserverViewsKey);
    dialog_host_observer->OnPositionRequiresUpdate();
  }
  return widget;
}
