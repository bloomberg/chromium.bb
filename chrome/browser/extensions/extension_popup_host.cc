// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_popup_host.h"

#include "base/message_loop.h"
#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/extensions/extension_popup_api.h"
#endif
#include "chrome/browser/profile.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/views/extensions/extension_popup.h"
#endif
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#if defined(TOOLKIT_VIEWS)
#include "views/focus/focus_manager.h"
#include "views/widget/root_view.h"
#endif

#if defined(TOOLKIT_VIEWS)
// A helper class that monitors native focus change events.  Because the views
// FocusManager does not listen for view-change notification across
// native-windows, we need to use native-listener utilities.
class ExtensionPopupHost::PopupFocusListener
    : public views::WidgetFocusChangeListener {
 public:
  // Constructs and registers a new PopupFocusListener for the given
  // |popup_host|.
  explicit PopupFocusListener(ExtensionPopupHost* popup_host)
      : popup_host_(popup_host) {
    views::FocusManager::GetWidgetFocusManager()->AddFocusChangeListener(this);
  }

  virtual ~PopupFocusListener() {
    views::FocusManager::GetWidgetFocusManager()->
        RemoveFocusChangeListener(this);
  }

  virtual void NativeFocusWillChange(gfx::NativeView focused_before,
                                     gfx::NativeView focused_now) {
    // If no view is to be focused, then Chrome was deactivated, so hide the
    // popup.
    if (!focused_now) {
      popup_host_->DismissPopupAsync();
      return;
    }

    gfx::NativeView host_view = popup_host_->delegate()->GetNativeViewOfHost();

    // If the widget hosting the popup contains the newly focused view, then
    // don't dismiss the pop-up.
    views::Widget* popup_root_widget =
        popup_host_->child_popup()->host()->view()->GetWidget();
    if (popup_root_widget &&
        popup_root_widget->ContainsNativeView(focused_now)) {
      return;
    }

    // If the widget or RenderWidgetHostView hosting the extension that
    // launched the pop-up is receiving focus, then don't dismiss the popup.
    views::Widget* host_widget =
        views::Widget::GetWidgetFromNativeView(host_view);
    if (host_widget && host_widget->ContainsNativeView(focused_now)) {
      return;
    }

    RenderWidgetHostView* render_host_view =
        RenderWidgetHostView::GetRenderWidgetHostViewFromNativeView(
            host_view);
    if (render_host_view &&
        render_host_view->ContainsNativeView(focused_now)) {
      return;
    }

    popup_host_->DismissPopupAsync();
  }

 private:
  ExtensionPopupHost* popup_host_;
  DISALLOW_COPY_AND_ASSIGN(PopupFocusListener);
};
#endif  // if defined(TOOLKIT_VIEWS)


ExtensionPopupHost::PopupDelegate::~PopupDelegate() {
  // If the PopupDelegate is being torn down, then make sure to reset the
  // cached pointer in the host to prevent access to a stale pointer.
  if (popup_host_.get())
    popup_host_->RevokeDelegate();
}

ExtensionPopupHost* ExtensionPopupHost::PopupDelegate::popup_host() {
  if (!popup_host_.get())
    popup_host_.reset(new ExtensionPopupHost(this));

  return popup_host_.get();
}

Profile* ExtensionPopupHost::PopupDelegate::GetProfile() {
  // If there is a browser present, return the profile associated with it.
  // When hosting a view in an ExternalTabContainer, it is possible to have
  // no Browser instance.
  Browser* browser = GetBrowser();
  if (browser) {
    return browser->profile();
  }

  return NULL;
}

ExtensionPopupHost::ExtensionPopupHost(PopupDelegate* delegate)
    :  // NO LINT
#if defined(TOOLKIT_VIEWS)
      listener_(NULL),
      child_popup_(NULL),
#endif
      delegate_(delegate),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
  DCHECK(delegate_);

  // Listen for view close requests, so that we can dismiss a hosted pop-up
  // view, if necessary.
  Profile* profile = delegate_->GetProfile();
  DCHECK(profile);
  registrar_.Add(this, NotificationType::EXTENSION_HOST_VIEW_SHOULD_CLOSE,
                 Source<Profile>(profile));
}

ExtensionPopupHost::~ExtensionPopupHost() {
  DismissPopup();
}

#if defined(TOOLKIT_VIEWS)
void ExtensionPopupHost::set_child_popup(ExtensionPopup* popup) {
  // An extension may only have one popup active at a given time.
  DismissPopup();
  if (popup)
    listener_.reset(new PopupFocusListener(this));

  child_popup_ = popup;
}

void ExtensionPopupHost::BubbleBrowserWindowMoved(BrowserBubble* bubble) {
  DismissPopupAsync();
}

void ExtensionPopupHost::BubbleBrowserWindowClosing(BrowserBubble* bubble) {
  DismissPopupAsync();
}
#endif  // defined(TOOLKIT_VIEWS)

void ExtensionPopupHost::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  if (type == NotificationType::EXTENSION_HOST_VIEW_SHOULD_CLOSE) {
#if defined(TOOLKIT_VIEWS)
    // If we aren't the host of the popup, then disregard the notification.
    if (!child_popup_ ||
        Details<ExtensionHost>(child_popup_->host()) != details) {
      return;
    }
    DismissPopup();
#endif
  } else {
    NOTREACHED();
  }
}

void ExtensionPopupHost::DismissPopup() {
#if defined(TOOLKIT_VIEWS)
  listener_.reset(NULL);
  if (child_popup_) {
    child_popup_->Hide();
    child_popup_->DetachFromBrowser();
    delete child_popup_;
    child_popup_ = NULL;

    if (delegate_) {
      PopupEventRouter::OnPopupClosed(
          delegate_->GetProfile(),
          delegate_->GetRenderViewHost()->routing_id());
    }
  }
#endif  // defined(TOOLKIT_VIEWS)
}

void ExtensionPopupHost::DismissPopupAsync() {
  // Dismiss the popup asynchronously, as we could be deep in a message loop
  // processing activations, and the focus manager may get confused if the
  // currently focused view is destroyed.
  method_factory_.RevokeAll();
  MessageLoop::current()->PostNonNestableTask(
      FROM_HERE,
      method_factory_.NewRunnableMethod(
          &ExtensionPopupHost::DismissPopup));
}
