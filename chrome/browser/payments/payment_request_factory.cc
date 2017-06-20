// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/payment_request_factory.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/payments/chrome_payment_request_delegate.h"
#include "components/payments/content/payment_request_web_contents_manager.h"

namespace payments {

void CreatePaymentRequest(const service_manager::BindSourceInfo& source_info,
                          mojom::PaymentRequestRequest request,
                          content::RenderFrameHost* render_frame_host) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents)
    return;
  PaymentRequestWebContentsManager::GetOrCreateForWebContents(web_contents)
      ->CreatePaymentRequest(
          render_frame_host, web_contents,
          base::MakeUnique<ChromePaymentRequestDelegate>(web_contents),
          std::move(request),
          /*observer_for_testing=*/nullptr);
}

}  // namespace payments
