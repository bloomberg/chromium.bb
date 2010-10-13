// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/constrained_html_dialog.h"

#include "chrome/browser/dom_ui/html_dialog_ui.h"
#include "chrome/browser/renderer_host/render_view_host.h"

ConstrainedHtmlDialog::ConstrainedHtmlDialog(Profile* profile,
                                             HtmlDialogUIDelegate* delegate)
    : DOMUI(NULL),
      profile_(profile),
      render_view_host_(NULL),
      html_dialog_ui_delegate_(delegate) {
}

ConstrainedHtmlDialog::~ConstrainedHtmlDialog() {
}

void ConstrainedHtmlDialog::InitializeDOMUI(RenderViewHost* render_view_host) {
  render_view_host_ = render_view_host;

  std::vector<DOMMessageHandler*> handlers;
  html_dialog_ui_delegate_->GetDOMMessageHandlers(&handlers);
  render_view_host->SetDOMUIProperty("dialogArguments",
      html_dialog_ui_delegate_->GetDialogArgs());
  for (std::vector<DOMMessageHandler*>::iterator it = handlers.begin();
       it != handlers.end(); ++it) {
    (*it)->Attach(this);
    AddMessageHandler(*it);
  }
}
