// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_passwords_bubble_delegate_view_base.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/passwords/manage_password_auto_sign_in_view.h"
#include "chrome/browser/ui/views/passwords/manage_password_items_view.h"
#include "chrome/browser/ui/views/passwords/manage_password_save_confirmation_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_bubble_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_icon_views.h"
#include "ui/base/material_design/material_design_controller.h"

#if !defined(OS_MACOSX) || BUILDFLAG(MAC_VIEWS_BROWSER)
#include "chrome/browser/ui/views/frame/browser_view.h"
#endif

// static
ManagePasswordsBubbleDelegateViewBase*
    ManagePasswordsBubbleDelegateViewBase::g_manage_passwords_bubble_ = nullptr;

#if !defined(OS_MACOSX) || BUILDFLAG(MAC_VIEWS_BROWSER)

// static
void ManagePasswordsBubbleDelegateViewBase::ShowBubble(
    content::WebContents* web_contents,
    DisplayReason reason) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  DCHECK(browser);
  DCHECK(browser->window());
  DCHECK(!g_manage_passwords_bubble_ ||
         !g_manage_passwords_bubble_->GetWidget()->IsVisible());

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  bool is_fullscreen = browser_view->IsFullscreen();
  views::View* anchor_view = nullptr;
  if (!is_fullscreen) {
    if (ui::MaterialDesignController::IsSecondaryUiMaterial()) {
      anchor_view = browser_view->GetLocationBarView();
    } else {
      anchor_view =
          browser_view->GetLocationBarView()->manage_passwords_icon_view();
    }
  }

  ManagePasswordsBubbleDelegateViewBase* bubble =
      CreateBubble(web_contents, anchor_view, gfx::Point(), reason);
  DCHECK(bubble);
  DCHECK(bubble == g_manage_passwords_bubble_);

  if (is_fullscreen)
    g_manage_passwords_bubble_->set_parent_window(
        web_contents->GetNativeView());

  views::Widget* bubble_widget =
      views::BubbleDialogDelegateView::CreateBubble(g_manage_passwords_bubble_);

  if (anchor_view) {
    bubble_widget->AddObserver(
        browser_view->GetLocationBarView()->manage_passwords_icon_view());
  }

  // Adjust for fullscreen after creation as it relies on the content size.
  if (is_fullscreen) {
    g_manage_passwords_bubble_->AdjustForFullscreen(
        browser_view->GetBoundsInScreen());
  }

  g_manage_passwords_bubble_->ShowForReason(reason);
}

#endif  // !defined(OS_MACOSX) || BUILDFLAG(MAC_VIEWS_BROWSER)

// static
ManagePasswordsBubbleDelegateViewBase*
ManagePasswordsBubbleDelegateViewBase::CreateBubble(
    content::WebContents* web_contents,
    views::View* anchor_view,
    const gfx::Point& anchor_point,
    DisplayReason reason) {
  ManagePasswordsBubbleDelegateViewBase* view;
  password_manager::ui::State model_state =
      PasswordsModelDelegateFromWebContents(web_contents)->GetState();
  if (model_state == password_manager::ui::MANAGE_STATE) {
    view = new ManagePasswordItemsView(web_contents, anchor_view, anchor_point,
                                       reason);
  } else if (model_state == password_manager::ui::AUTO_SIGNIN_STATE) {
    view = new ManagePasswordAutoSignInView(web_contents, anchor_view,
                                            anchor_point, reason);
  } else if (model_state == password_manager::ui::CONFIRMATION_STATE) {
    view = new ManagePasswordSaveConfirmationView(web_contents, anchor_view,
                                                  anchor_point, reason);
  } else {
    // TODO(crbug.com/654115): Get rid of the one-bubble-for-everything
    // BubbleView.
    view = new ManagePasswordsBubbleView(web_contents, anchor_view,
                                         anchor_point, reason);
  }

  g_manage_passwords_bubble_ = view;
  return view;
}

// static
void ManagePasswordsBubbleDelegateViewBase::CloseCurrentBubble() {
  if (g_manage_passwords_bubble_)
    g_manage_passwords_bubble_->GetWidget()->Close();
}

// static
void ManagePasswordsBubbleDelegateViewBase::ActivateBubble() {
  DCHECK(g_manage_passwords_bubble_);
  DCHECK(g_manage_passwords_bubble_->GetWidget()->IsVisible());
  g_manage_passwords_bubble_->GetWidget()->Activate();
}

const content::WebContents*
ManagePasswordsBubbleDelegateViewBase::GetWebContents() const {
  return model_.GetWebContents();
}

base::string16 ManagePasswordsBubbleDelegateViewBase::GetWindowTitle() const {
  return model_.title();
}

bool ManagePasswordsBubbleDelegateViewBase::ShouldShowWindowTitle() const {
  return !model_.title().empty();
}

ManagePasswordsBubbleDelegateViewBase::ManagePasswordsBubbleDelegateViewBase(
    content::WebContents* web_contents,
    views::View* anchor_view,
    const gfx::Point& anchor_point,
    DisplayReason reason)
    : LocationBarBubbleDelegateView(anchor_view, anchor_point, web_contents),
      model_(PasswordsModelDelegateFromWebContents(web_contents),
             reason == AUTOMATIC ? ManagePasswordsBubbleModel::AUTOMATIC
                                 : ManagePasswordsBubbleModel::USER_ACTION),
      mouse_handler_(
          std::make_unique<WebContentMouseHandler>(this, web_contents)) {}

ManagePasswordsBubbleDelegateViewBase::
    ~ManagePasswordsBubbleDelegateViewBase() {
  if (g_manage_passwords_bubble_ == this)
    g_manage_passwords_bubble_ = nullptr;
}

void ManagePasswordsBubbleDelegateViewBase::OnWidgetClosing(
    views::Widget* widget) {
  LocationBarBubbleDelegateView::OnWidgetClosing(widget);
  if (widget != GetWidget())
    return;
  mouse_handler_.reset();
  // It can be the case that a password bubble is being closed while another
  // password bubble is being opened. The metrics recorder can be shared between
  // them and it doesn't understand the sequence [open1, open2, close1, close2].
  // Therefore, we reset the model early (before the bubble destructor) to get
  // the following sequence of events [open1, close1, open2, close2].
  model_.OnBubbleClosing();
}
