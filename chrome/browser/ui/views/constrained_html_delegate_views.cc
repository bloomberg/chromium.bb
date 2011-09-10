// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/constrained_html_ui.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "chrome/browser/ui/views/tab_contents/tab_contents_container.h"
#include "chrome/browser/ui/webui/html_dialog_tab_contents_delegate.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "ui/gfx/rect.h"
#include "views/view.h"
#include "views/widget/widget_delegate.h"

class ConstrainedHtmlDelegateViews : public TabContentsContainer,
                                     public ConstrainedHtmlUIDelegate,
                                     public views::WidgetDelegate,
                                     public HtmlDialogTabContentsDelegate {
 public:
  ConstrainedHtmlDelegateViews(Profile* profile,
                               HtmlDialogUIDelegate* delegate);
  ~ConstrainedHtmlDelegateViews();

  // ConstrainedHtmlUIDelegate interface.
  virtual HtmlDialogUIDelegate* GetHtmlDialogUIDelegate() OVERRIDE;
  virtual void OnDialogCloseFromWebUI() OVERRIDE;

  // views::WidgetDelegate interface.
  virtual bool CanResize() const OVERRIDE { return true; }
  virtual views::View* GetContentsView() OVERRIDE {
    return this;
  }
  virtual void WindowClosing() OVERRIDE {
    if (!closed_via_webui_)
      html_delegate_->OnDialogClosed("");
  }
  virtual views::Widget* GetWidget() OVERRIDE {
    return View::GetWidget();
  }
  virtual const views::Widget* GetWidget() const OVERRIDE {
    return View::GetWidget();
  }

  virtual std::wstring GetWindowTitle() const OVERRIDE {
    return UTF16ToWideHack(html_delegate_->GetDialogTitle());
  }

  // HtmlDialogTabContentsDelegate interface.
  void HandleKeyboardEvent(const NativeWebKeyboardEvent& event) OVERRIDE {}

  // Overridden from TabContentsContainer.
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    gfx::Size size;
    html_delegate_->GetDialogSize(&size);
    return size;
  }

  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child) OVERRIDE {
    TabContentsContainer::ViewHierarchyChanged(is_add, parent, child);
    if (is_add && child == this) {
      ChangeTabContents(&html_tab_contents_);
    }
  }

  void set_window(ConstrainedWindow* window) {
    window_ = window;
  }

 private:
  TabContents html_tab_contents_;

  HtmlDialogUIDelegate* html_delegate_;

  // The constrained window that owns |this|.  Saved so we can close it later.
  ConstrainedWindow* window_;

  // Was the dialog closed from WebUI (in which case |html_delegate_|'s
  // OnDialogClosed() method has already been called)?
  bool closed_via_webui_;
};

ConstrainedHtmlDelegateViews::ConstrainedHtmlDelegateViews(
    Profile* profile,
    HtmlDialogUIDelegate* delegate)
    : HtmlDialogTabContentsDelegate(profile),
      html_tab_contents_(profile, NULL, MSG_ROUTING_NONE, NULL, NULL),
      html_delegate_(delegate),
      window_(NULL),
      closed_via_webui_(false) {
  CHECK(delegate);
  html_tab_contents_.set_delegate(this);

  // Set |this| as a property so the ConstrainedHtmlUI can retrieve it.
  ConstrainedHtmlUI::GetPropertyAccessor().SetProperty(
      html_tab_contents_.property_bag(), this);
  html_tab_contents_.controller().LoadURL(delegate->GetDialogContentURL(),
                                          GURL(),
                                          PageTransition::START_PAGE,
                                          std::string());
}

ConstrainedHtmlDelegateViews::~ConstrainedHtmlDelegateViews() {
}

HtmlDialogUIDelegate* ConstrainedHtmlDelegateViews::GetHtmlDialogUIDelegate() {
  return html_delegate_;
}

void ConstrainedHtmlDelegateViews::OnDialogCloseFromWebUI() {
  closed_via_webui_ = true;
  window_->CloseConstrainedWindow();
}

// static
ConstrainedWindow* ConstrainedHtmlUI::CreateConstrainedHtmlDialog(
    Profile* profile,
    HtmlDialogUIDelegate* delegate,
    TabContents* container) {
  ConstrainedHtmlDelegateViews* constrained_delegate =
      new ConstrainedHtmlDelegateViews(profile, delegate);
  ConstrainedWindow* constrained_window =
      new ConstrainedWindowViews(container, constrained_delegate);
  constrained_delegate->set_window(constrained_window);
  return constrained_window;
}
