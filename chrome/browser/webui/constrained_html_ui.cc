// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webui/constrained_html_ui.h"

#include "base/lazy_instance.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/webui/html_dialog_ui.h"
#include "chrome/browser/webui/web_ui_util.h"
#include "chrome/common/bindings_policy.h"

static base::LazyInstance<PropertyAccessor<ConstrainedHtmlUIDelegate*> >
    g_constrained_html_ui_property_accessor(base::LINKER_INITIALIZED);

ConstrainedHtmlUI::ConstrainedHtmlUI(TabContents* contents)
    : WebUI(contents) {
}

ConstrainedHtmlUI::~ConstrainedHtmlUI() {
}

void ConstrainedHtmlUI::RenderViewCreated(
    RenderViewHost* render_view_host) {
  ConstrainedHtmlUIDelegate* delegate = GetConstrainedDelegate();
  if (!delegate)
    return;

  HtmlDialogUIDelegate* dialog_delegate = delegate->GetHtmlDialogUIDelegate();
  std::vector<WebUIMessageHandler*> handlers;
  dialog_delegate->GetWebUIMessageHandlers(&handlers);
  render_view_host->SetWebUIProperty("dialogArguments",
                                     dialog_delegate->GetDialogArgs());
  for (std::vector<WebUIMessageHandler*>::iterator it = handlers.begin();
       it != handlers.end(); ++it) {
    (*it)->Attach(this);
    AddMessageHandler(*it);
  }

  // Add a "DialogClose" callback which matches HTMLDialogUI behavior.
  RegisterMessageCallback("DialogClose",
      NewCallback(this, &ConstrainedHtmlUI::OnDialogClose));
}

void ConstrainedHtmlUI::OnDialogClose(const ListValue* args) {
  ConstrainedHtmlUIDelegate* delegate = GetConstrainedDelegate();
  if (!delegate)
    return;

  delegate->GetHtmlDialogUIDelegate()->OnDialogClosed(
      web_ui_util::GetJsonResponseFromFirstArgumentInList(args));
  delegate->OnDialogClose();
}

ConstrainedHtmlUIDelegate*
    ConstrainedHtmlUI::GetConstrainedDelegate() {
  ConstrainedHtmlUIDelegate** property =
      GetPropertyAccessor().GetProperty(tab_contents()->property_bag());
  return property ? *property : NULL;
}

// static
PropertyAccessor<ConstrainedHtmlUIDelegate*>&
    ConstrainedHtmlUI::GetPropertyAccessor() {
  return g_constrained_html_ui_property_accessor.Get();
}
