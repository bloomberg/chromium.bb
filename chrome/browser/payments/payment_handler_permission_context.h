// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAYMENTS_PAYMENT_HANDLER_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_PAYMENTS_PAYMENT_HANDLER_PERMISSION_CONTEXT_H_

#include "base/macros.h"
#include "chrome/browser/permissions/permission_context_base.h"

class Profile;

namespace payments {

class PaymentHandlerPermissionContext : public PermissionContextBase {
 public:
  explicit PaymentHandlerPermissionContext(Profile* profile);
  ~PaymentHandlerPermissionContext() override;

 private:
  // PermissionContextBase
  bool IsRestrictedToSecureOrigins() const override;

  DISALLOW_COPY_AND_ASSIGN(PaymentHandlerPermissionContext);
};

}  // namespace payments

#endif  // CHROME_BROWSER_PAYMENTS_PAYMENT_HANDLER_PERMISSION_CONTEXT_H_
