// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/constrained_html_ui.h"

#include "chrome/browser/dom_ui/html_dialog_tab_contents_delegate.h"
#include "chrome/browser/dom_ui/html_dialog_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/views/tab_contents/tab_contents_container.h"
#include "ipc/ipc_message.h"
#include "ui/gfx/rect.h"
#include "views/view.h"
#include "views/widget/widget_win.h"
#include "views/window/window_delegate.h"

class ConstrainedHtmlDelegateWin : public TabContentsContainer,
                                   public ConstrainedHtmlUIDelegate,
                                   public ConstrainedWindowDelegate,
                                   public HtmlDialogTabContentsDelegate {
 public:
  ConstrainedHtmlDelegateWin(Profile* profile,
                             HtmlDialogUIDelegate* delegate);
  ~ConstrainedHtmlDelegateWin();

  // ConstrainedHtmlUIDelegate interface.
  virtual HtmlDialogUIDelegate* GetHtmlDialogUIDelegate();
  virtual void OnDialogClose();

  // ConstrainedWindowDelegate (aka views::WindowDelegate) interface.
  virtual bool CanResize() const { return true; }
  virtual views::View* GetContentsView() {
    return this;
  }
  virtual void WindowClosing() {
    html_delegate_->OnDialogClosed("");
  }

  // HtmlDialogTabContentsDelegate interface.
  void MoveContents(TabContents* source, const gfx::Rect& pos) {}
  void ToolbarSizeChanged(TabContents* source, bool is_animating) {}
  void HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {}

  // Overridden from TabContentsContainer.
  virtual gfx::Size GetPreferredSize() {
    gfx::Size size;
    html_delegate_->GetDialogSize(&size);
    return size;
  }

  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child) {
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
};

ConstrainedHtmlDelegateWin::ConstrainedHtmlDelegateWin(
    Profile* profile,
    HtmlDialogUIDelegate* delegate)
    : HtmlDialogTabContentsDelegate(profile),
      html_tab_contents_(profile, NULL, MSG_ROUTING_NONE, NULL, NULL),
      html_delegate_(delegate),
      window_(NULL) {
  CHECK(delegate);
  html_tab_contents_.set_delegate(this);

  // Set |this| as a property so the ConstrainedHtmlUI can retrieve it.
  ConstrainedHtmlUI::GetPropertyAccessor().SetProperty(
      html_tab_contents_.property_bag(), this);
  html_tab_contents_.controller().LoadURL(delegate->GetDialogContentURL(),
                                          GURL(),
                                          PageTransition::START_PAGE);
}

ConstrainedHtmlDelegateWin::~ConstrainedHtmlDelegateWin() {
}

HtmlDialogUIDelegate* ConstrainedHtmlDelegateWin::GetHtmlDialogUIDelegate() {
  return html_delegate_;
}

void ConstrainedHtmlDelegateWin::OnDialogClose() {
  window_->CloseConstrainedWindow();
}

// static
void ConstrainedHtmlUI::CreateConstrainedHtmlDialog(
    Profile* profile,
    HtmlDialogUIDelegate* delegate,
    TabContents* container) {
  ConstrainedHtmlDelegateWin* constrained_delegate =
      new ConstrainedHtmlDelegateWin(profile, delegate);
  ConstrainedWindow* constrained_window =
      container->CreateConstrainedDialog(constrained_delegate);
  constrained_delegate->set_window(constrained_window);
}
