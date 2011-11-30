// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/html_dialog_ui.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/property_bag.h"
#include "base/values.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/bindings_policy.h"

static base::LazyInstance<base::PropertyAccessor<HtmlDialogUIDelegate*> >
    g_html_dialog_ui_property_accessor = LAZY_INSTANCE_INITIALIZER;

HtmlDialogUI::HtmlDialogUI(TabContents* tab_contents)
    : ChromeWebUI(tab_contents) {
}

HtmlDialogUI::~HtmlDialogUI() {
  // Don't unregister our property. During the teardown of the TabContents,
  // this will be deleted, but the TabContents will already be destroyed.
  //
  // This object is owned indirectly by the TabContents. WebUIs can change, so
  // it's scary if this WebUI is changed out and replaced with something else,
  // since the property will still point to the old delegate. But the delegate
  // is itself the owner of the TabContents for a dialog so will be in scope,
  // and the HTML dialogs won't swap WebUIs anyway since they don't navigate.
}

void HtmlDialogUI::CloseDialog(const base::ListValue* args) {
  OnDialogClosed(args);
}

// static
base::PropertyAccessor<HtmlDialogUIDelegate*>&
    HtmlDialogUI::GetPropertyAccessor() {
  return g_html_dialog_ui_property_accessor.Get();
}

////////////////////////////////////////////////////////////////////////////////
// Private:

void HtmlDialogUI::RenderViewCreated(RenderViewHost* render_view_host) {
  // Hook up the javascript function calls, also known as chrome.send("foo")
  // calls in the HTML, to the actual C++ functions.
  RegisterMessageCallback("DialogClose",
      base::Bind(&HtmlDialogUI::OnDialogClosed, base::Unretained(this)));

  // Pass the arguments to the renderer supplied by the delegate.
  std::string dialog_args;
  std::vector<WebUIMessageHandler*> handlers;
  HtmlDialogUIDelegate** delegate = GetPropertyAccessor().GetProperty(
      tab_contents()->property_bag());
  if (delegate) {
    dialog_args = (*delegate)->GetDialogArgs();
    (*delegate)->GetWebUIMessageHandlers(&handlers);
  }

  if (0 != (bindings_ & content::BINDINGS_POLICY_WEB_UI))
    render_view_host->SetWebUIProperty("dialogArguments", dialog_args);
  for (std::vector<WebUIMessageHandler*>::iterator it = handlers.begin();
       it != handlers.end(); ++it) {
    (*it)->Attach(this);
    AddMessageHandler(*it);
  }

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_HTML_DIALOG_SHOWN,
      content::Source<WebUI>(this),
      content::Details<RenderViewHost>(render_view_host));
}

void HtmlDialogUI::OnDialogClosed(const ListValue* args) {
  HtmlDialogUIDelegate** delegate = GetPropertyAccessor().GetProperty(
      tab_contents()->property_bag());
  if (delegate) {
    std::string json_retval;
    if (args && !args->empty() && !args->GetString(0, &json_retval))
      NOTREACHED() << "Could not read JSON argument";

    (*delegate)->OnDialogClosed(json_retval);
  }
}

ExternalHtmlDialogUI::ExternalHtmlDialogUI(TabContents* tab_contents)
    : HtmlDialogUI(tab_contents) {
  // Non-file based UI needs to not have access to the Web UI bindings
  // for security reasons. The code hosting the dialog should provide
  // dialog specific functionality through other bindings and methods
  // that are scoped in duration to the dialogs existence.
  bindings_ &= ~content::BINDINGS_POLICY_WEB_UI;
}

ExternalHtmlDialogUI::~ExternalHtmlDialogUI() {
}

bool HtmlDialogUIDelegate::HandleContextMenu(const ContextMenuParams& params) {
  return false;
}
