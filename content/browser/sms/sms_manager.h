// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMS_SMS_MANAGER_H_
#define CONTENT_BROWSER_SMS_SMS_MANAGER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "content/browser/sms/sms_provider.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "third_party/blink/public/mojom/sms/sms_manager.mojom.h"

namespace url {
class Origin;
}

namespace content {

// The SmsManager is responsible for taking the incoming mojo calls from the
// renderer process and dispatching them to the SmsProvider platform-specific
// implementation.
class CONTENT_EXPORT SmsManager : public blink::mojom::SmsManager {
 public:
  SmsManager();
  ~SmsManager() override;

  void CreateService(blink::mojom::SmsManagerRequest request,
                     const url::Origin& origin);

  // blink.mojom.SmsManager:
  void GetNextMessage(base::TimeDelta timeout,
                      GetNextMessageCallback callback) override;

  // Testing helpers.
  void SetSmsProviderForTest(std::unique_ptr<SmsProvider> sms_provider);

 private:
  std::unique_ptr<SmsProvider> sms_provider_;

  // Registered clients.
  mojo::BindingSet<blink::mojom::SmsManager> bindings_;
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(SmsManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMS_SMS_MANAGER_H_
