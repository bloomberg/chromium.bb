// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/payment_handler_permission_context.h"

#include "base/notreached.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_request_id.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom.h"
#include "url/gurl.h"

namespace payments {

PaymentHandlerPermissionContext::PaymentHandlerPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(browser_context,
                            ContentSettingsType::PAYMENT_HANDLER,
                            blink::mojom::FeaturePolicyFeature::kNotFound) {}

PaymentHandlerPermissionContext::~PaymentHandlerPermissionContext() {}

void PaymentHandlerPermissionContext::DecidePermission(
    content::WebContents* web_contents,
    const permissions::PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    bool user_gesture,
    permissions::BrowserPermissionCallback callback) {
  // The user should never be prompted to authorize payment handler.
  NOTREACHED();
}

bool PaymentHandlerPermissionContext::IsRestrictedToSecureOrigins() const {
  return true;
}

}  // namespace payments
