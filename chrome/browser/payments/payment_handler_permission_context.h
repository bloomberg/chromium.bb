// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAYMENTS_PAYMENT_HANDLER_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_PAYMENTS_PAYMENT_HANDLER_PERMISSION_CONTEXT_H_

#include "base/macros.h"
#include "components/permissions/permission_context_base.h"

class GURL;

namespace permissions {
class PermissionRequestID;
}

namespace content {
class WebContents;
}  // namespace content

namespace payments {

class PaymentHandlerPermissionContext
    : public permissions::PermissionContextBase {
 public:
  explicit PaymentHandlerPermissionContext(
      content::BrowserContext* browser_context);
  ~PaymentHandlerPermissionContext() override;

 private:
  // PermissionContextBase
  void DecidePermission(
      content::WebContents* web_contents,
      const permissions::PermissionRequestID& id,
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      bool user_gesture,
      permissions::BrowserPermissionCallback callback) override;
  bool IsRestrictedToSecureOrigins() const override;

  DISALLOW_COPY_AND_ASSIGN(PaymentHandlerPermissionContext);
};

}  // namespace payments

#endif  // CHROME_BROWSER_PAYMENTS_PAYMENT_HANDLER_PERMISSION_CONTEXT_H_
