// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/testing_automation_provider.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/automation/automation_browser_tracker.h"
#include "chrome/browser/automation/automation_window_tracker.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar_view.h"
#include "chrome/common/automation_messages.h"
#include "ui/gfx/point.h"
#include "ui/views/widget/widget.h"
#include "views/controls/menu/menu_listener.h"
#include "views/view.h"

namespace {

// Helper class that waits until the focus has changed to a view other
// than the one with the provided view id.
class ViewFocusChangeWaiter : public views::FocusChangeListener {
 public:
  ViewFocusChangeWaiter(views::FocusManager* focus_manager,
                        int previous_view_id,
                        AutomationProvider* automation,
                        IPC::Message* reply_message)
      : focus_manager_(focus_manager),
        previous_view_id_(previous_view_id),
        automation_(automation),
        reply_message_(reply_message),
        ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
    focus_manager_->AddFocusChangeListener(this);
    // Call the focus change notification once in case the focus has
    // already changed.
    OnWillChangeFocus(NULL, focus_manager_->GetFocusedView());
  }

  virtual ~ViewFocusChangeWaiter() {
    focus_manager_->RemoveFocusChangeListener(this);
  }

  // Inherited from FocusChangeListener
  virtual void OnWillChangeFocus(views::View* focused_before,
                                 views::View* focused_now) {
  }

  virtual void OnDidChangeFocus(views::View* focused_before,
                                views::View* focused_now) {
    if (focused_now && focused_now->id() != previous_view_id_) {
      AutomationMsg_WaitForFocusedViewIDToChange::WriteReplyParams(
          reply_message_, true, focused_now->id());

      automation_->Send(reply_message_);
      delete this;
    }
  }

 private:
  views::FocusManager* focus_manager_;
  int previous_view_id_;
  AutomationProvider* automation_;
  IPC::Message* reply_message_;
  base::WeakPtrFactory<ViewFocusChangeWaiter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ViewFocusChangeWaiter);
};

}  // namespace

class TestingAutomationProvider::PopupMenuWaiter : public views::MenuListener {
 public:
  PopupMenuWaiter(ToolbarView* toolbar_view,
                  TestingAutomationProvider* automation)
      : toolbar_view_(toolbar_view),
        automation_(automation),
        reply_message_(NULL) {
    toolbar_view_->AddMenuListener(this);
  }

  virtual ~PopupMenuWaiter() {}

  // Overridden from views::MenuListener:
  virtual void OnMenuOpened() OVERRIDE {
    toolbar_view_->RemoveMenuListener(this);
    automation_->popup_menu_opened_ = true;
    automation_->popup_menu_waiter_ = NULL;
    if (reply_message_) {
      AutomationMsg_WaitForPopupMenuToOpen::WriteReplyParams(
          reply_message_, true);
      automation_->Send(reply_message_);
    }
    delete this;
  }

  void set_reply_message(IPC::Message* reply_message) {
    reply_message_ = reply_message;
  }

 private:
  ToolbarView* toolbar_view_;
  TestingAutomationProvider* automation_;
  IPC::Message* reply_message_;

  DISALLOW_COPY_AND_ASSIGN(PopupMenuWaiter);
};

void TestingAutomationProvider::WindowGetViewBounds(int handle,
                                                    int view_id,
                                                    bool screen_coordinates,
                                                    bool* success,
                                                    gfx::Rect* bounds) {
  *success = false;

  if (window_tracker_->ContainsHandle(handle)) {
    gfx::NativeWindow window = window_tracker_->GetResource(handle);
    views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
    if (widget) {
      views::View* root_view = widget->GetRootView();
      views::View* view = root_view->GetViewByID(view_id);
      if (view) {
        *success = true;
        gfx::Point point;
        if (screen_coordinates)
          views::View::ConvertPointToScreen(view, &point);
        else
          views::View::ConvertPointToView(view, root_view, &point);
        *bounds = view->GetContentsBounds();
        bounds->set_origin(point);
      }
    }
  }
}

void TestingAutomationProvider::GetFocusedViewID(int handle, int* view_id) {
  *view_id = -1;
  if (window_tracker_->ContainsHandle(handle)) {
    gfx::NativeWindow window = window_tracker_->GetResource(handle);
    const views::Widget* widget =
        views::Widget::GetWidgetForNativeWindow(window);
    DCHECK(widget);
    const views::FocusManager* focus_manager = widget->GetFocusManager();
    DCHECK(focus_manager);
    const views::View* focused_view = focus_manager->GetFocusedView();
    if (focused_view)
      *view_id = focused_view->id();
  }
}

void TestingAutomationProvider::WaitForFocusedViewIDToChange(
    int handle, int previous_view_id, IPC::Message* reply_message) {
  if (!window_tracker_->ContainsHandle(handle))
    return;
  gfx::NativeWindow window = window_tracker_->GetResource(handle);
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  DCHECK(widget);
  views::FocusManager* focus_manager = widget->GetFocusManager();

  // The waiter will respond to the IPC and delete itself when done.
  new ViewFocusChangeWaiter(focus_manager,
                            previous_view_id,
                            this,
                            reply_message);
}

void TestingAutomationProvider::StartTrackingPopupMenus(
    int browser_handle, bool* success) {
  if (browser_tracker_->ContainsHandle(browser_handle)) {
    Browser* browser = browser_tracker_->GetResource(browser_handle);
    BrowserView* browser_view = reinterpret_cast<BrowserView*>(
        browser->window());
    ToolbarView* toolbar_view = browser_view->GetToolbarView();
    popup_menu_opened_ = false;
    popup_menu_waiter_ = new PopupMenuWaiter(toolbar_view, this);
    *success = true;
  }
}

void TestingAutomationProvider::WaitForPopupMenuToOpen(
    IPC::Message* reply_message) {
  // See if the menu already opened and return true if so.
  if (popup_menu_opened_) {
    AutomationMsg_WaitForPopupMenuToOpen::WriteReplyParams(
          reply_message, true);
    Send(reply_message);
    return;
  }

  // Otherwise, register this reply message with the waiter,
  // which will handle responding to this IPC when the popup
  // menu opens.
  popup_menu_waiter_->set_reply_message(reply_message);
}
