// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/constrained_html_ui.h"

#include "base/lazy_instance.h"
#include "chrome/browser/dom_ui/html_dialog_ui.h"
#include "chrome/browser/dom_ui/web_ui_util.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/common/bindings_policy.h"

static base::LazyInstance<PropertyAccessor<ConstrainedHtmlUIDelegate*> >
    g_constrained_html_ui_property_accessor(base::LINKER_INITIALIZED);

ConstrainedHtmlUI::ConstrainedHtmlUI(TabContents* contents)
    : DOMUI(contents) {
}

ConstrainedHtmlUI::~ConstrainedHtmlUI() {
}

void ConstrainedHtmlUI::RenderViewCreated(
    RenderViewHost* render_view_host) {
  HtmlDialogUIDelegate* delegate =
      GetConstrainedDelegate()->GetHtmlDialogUIDelegate();
  std::vector<DOMMessageHandler*> handlers;
  delegate->GetDOMMessageHandlers(&handlers);
  render_view_host->SetDOMUIProperty("dialogArguments",
                                     delegate->GetDialogArgs());
  for (std::vector<DOMMessageHandler*>::iterator it = handlers.begin();
       it != handlers.end(); ++it) {
    (*it)->Attach(this);
    AddMessageHandler(*it);
  }

  // Add a "DialogClose" callback which matches HTMLDialogUI behavior.
  RegisterMessageCallback("DialogClose",
      NewCallback(this, &ConstrainedHtmlUI::OnDialogClose));
}

void ConstrainedHtmlUI::OnDialogClose(const ListValue* args) {
  GetConstrainedDelegate()->GetHtmlDialogUIDelegate()->OnDialogClosed(
      web_ui_util::GetJsonResponseFromFirstArgumentInList(args));
  GetConstrainedDelegate()->OnDialogClose();
}

ConstrainedHtmlUIDelegate*
    ConstrainedHtmlUI::GetConstrainedDelegate() {
  return *GetPropertyAccessor().GetProperty(tab_contents()->property_bag());
}

// static
PropertyAccessor<ConstrainedHtmlUIDelegate*>&
    ConstrainedHtmlUI::GetPropertyAccessor() {
  return g_constrained_html_ui_property_accessor.Get();
}
