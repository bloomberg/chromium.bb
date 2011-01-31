// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/html_dialog_ui.h"

#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/values.h"
#include "chrome/browser/dom_ui/web_ui_util.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/common/bindings_policy.h"

static base::LazyInstance<PropertyAccessor<HtmlDialogUIDelegate*> >
    g_html_dialog_ui_property_accessor(base::LINKER_INITIALIZED);

HtmlDialogUI::HtmlDialogUI(TabContents* tab_contents) : DOMUI(tab_contents) {
}

HtmlDialogUI::~HtmlDialogUI() {
  // Don't unregister our property. During the teardown of the TabContents,
  // this will be deleted, but the TabContents will already be destroyed.
  //
  // This object is owned indirectly by the TabContents. DOMUIs can change, so
  // it's scary if this DOMUI is changed out and replaced with something else,
  // since the property will still point to the old delegate. But the delegate
  // is itself the owner of the TabContents for a dialog so will be in scope,
  // and the HTML dialogs won't swap DOMUIs anyway since they don't navigate.
}

// static
PropertyAccessor<HtmlDialogUIDelegate*>& HtmlDialogUI::GetPropertyAccessor() {
  return g_html_dialog_ui_property_accessor.Get();
}

////////////////////////////////////////////////////////////////////////////////
// Private:

void HtmlDialogUI::RenderViewCreated(RenderViewHost* render_view_host) {
  // Hook up the javascript function calls, also known as chrome.send("foo")
  // calls in the HTML, to the actual C++ functions.
  RegisterMessageCallback("DialogClose",
                          NewCallback(this, &HtmlDialogUI::OnDialogClosed));

  // Pass the arguments to the renderer supplied by the delegate.
  std::string dialog_args;
  std::vector<DOMMessageHandler*> handlers;
  HtmlDialogUIDelegate** delegate = GetPropertyAccessor().GetProperty(
      tab_contents()->property_bag());
  if (delegate) {
    dialog_args = (*delegate)->GetDialogArgs();
    (*delegate)->GetDOMMessageHandlers(&handlers);
  }

  if (0 != (bindings_ & BindingsPolicy::DOM_UI))
    render_view_host->SetDOMUIProperty("dialogArguments", dialog_args);
  for (std::vector<DOMMessageHandler*>::iterator it = handlers.begin();
       it != handlers.end(); ++it) {
    (*it)->Attach(this);
    AddMessageHandler(*it);
  }
}

void HtmlDialogUI::OnDialogClosed(const ListValue* args) {
  HtmlDialogUIDelegate** delegate = GetPropertyAccessor().GetProperty(
      tab_contents()->property_bag());
  if (delegate) {
    (*delegate)->OnDialogClosed(
        web_ui_util::GetJsonResponseFromFirstArgumentInList(args));
  }
}

ExternalHtmlDialogUI::ExternalHtmlDialogUI(TabContents* tab_contents)
    : HtmlDialogUI(tab_contents) {
  // Non-file based UI needs to not have access to the DOM UI bindings
  // for security reasons. The code hosting the dialog should provide
  // dialog specific functionality through other bindings and methods
  // that are scoped in duration to the dialogs existence.
  bindings_ &= ~BindingsPolicy::DOM_UI;
}

ExternalHtmlDialogUI::~ExternalHtmlDialogUI() {
}

bool HtmlDialogUIDelegate::HandleContextMenu(const ContextMenuParams& params) {
  return false;
}
