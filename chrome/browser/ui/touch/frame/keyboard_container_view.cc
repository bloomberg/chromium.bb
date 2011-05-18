// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/touch/frame/keyboard_container_view.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/dom_view.h"
#include "chrome/common/url_constants.h"
#include "content/browser/site_instance.h"

namespace {

// Make the provided view and all of its descendents unfocusable.
void MakeViewHierarchyUnfocusable(views::View* view) {
  view->SetFocusable(false);
  for (int i = 0; i < view->child_count(); ++i) {
    MakeViewHierarchyUnfocusable(view->GetChildViewAt(i));
  }
}

}  // namepsace

// static
const char KeyboardContainerView::kViewClassName[] =
    "browser/ui/touch/frame/KeyboardContainerView";

///////////////////////////////////////////////////////////////////////////////
// KeyboardContainerView, public:

KeyboardContainerView::KeyboardContainerView(Profile* profile, Browser* browser)
    : dom_view_(new DOMView),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          extension_function_dispatcher_(profile, this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(tab_contents_registrar_(this)),
      browser_(browser) {
  GURL keyboard_url(chrome::kChromeUIKeyboardURL);
  dom_view_->Init(profile,
      SiteInstance::CreateSiteInstanceForURL(profile, keyboard_url));
  dom_view_->LoadURL(keyboard_url);

  dom_view_->SetVisible(true);
  AddChildView(dom_view_);

  // We have Inited the dom_view. So we must have a tab contents.
  tab_contents_registrar_.Observe(dom_view_->tab_contents());
}

KeyboardContainerView::~KeyboardContainerView() {
}

std::string KeyboardContainerView::GetClassName() const {
  return kViewClassName;
}

void KeyboardContainerView::Layout() {
  // TODO(bryeung): include a border between the keyboard and the client view
  dom_view_->SetBounds(0, 0, width(), height());
}

Browser* KeyboardContainerView::GetBrowser() {
  return browser_;
}

gfx::NativeView KeyboardContainerView::GetNativeViewOfHost() {
  return dom_view_->native_view();
}

TabContents* KeyboardContainerView::GetAssociatedTabContents() const {
  return dom_view_->tab_contents();
}

void KeyboardContainerView::ViewHierarchyChanged(bool is_add,
                                                 View* parent,
                                                 View* child) {
  if (is_add)
    MakeViewHierarchyUnfocusable(child);
}

bool KeyboardContainerView::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(KeyboardContainerView, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_Request, OnRequest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void KeyboardContainerView::OnRequest(
    const ExtensionHostMsg_Request_Params& request) {
  extension_function_dispatcher_.Dispatch(request,
      dom_view_->tab_contents()->render_view_host());
}
