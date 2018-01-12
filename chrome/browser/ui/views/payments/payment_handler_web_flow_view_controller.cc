// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_handler_web_flow_view_controller.h"

#include "base/base64.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_handle.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"

namespace payments {

PaymentHandlerWebFlowViewController::PaymentHandlerWebFlowViewController(
    PaymentRequestSpec* spec,
    PaymentRequestState* state,
    PaymentRequestDialogView* dialog,
    Profile* profile,
    GURL target)
    : PaymentRequestSheetController(spec, state, dialog),
      profile_(profile),
      target_(target) {}

PaymentHandlerWebFlowViewController::~PaymentHandlerWebFlowViewController() {}

base::string16 PaymentHandlerWebFlowViewController::GetSheetTitle() {
  return base::string16();
}

void PaymentHandlerWebFlowViewController::FillContentView(
    views::View* content_view) {
  content_view->SetLayoutManager(base::MakeUnique<views::FillLayout>());
  std::unique_ptr<views::WebView> web_view =
      std::make_unique<views::WebView>(profile_);

  web_view->LoadInitialURL(target_);
  // TODO(anthonyvd): Size to the actual available size in the dialog.
  web_view->SetPreferredSize(gfx::Size(100, 300));
  content_view->AddChildView(web_view.release());
}

}  // namespace payments
