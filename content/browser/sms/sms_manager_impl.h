// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMS_SMS_MANAGER_IMPL_H_
#define CONTENT_BROWSER_SMS_SMS_MANAGER_IMPL_H_

#include <memory>
#include <queue>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "content/browser/sms/sms_provider.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "third_party/blink/public/mojom/sms/sms_manager.mojom.h"
#include "url/origin.h"

namespace content {

class CONTENT_EXPORT SmsManagerImpl : public blink::mojom::SmsManager,
                                      public content::SmsProvider::Observer {
 public:
  SmsManagerImpl(SmsProvider*, const url::Origin&);
  ~SmsManagerImpl() override;

  // content::SmsProvider::Observer
  bool OnReceive(const url::Origin&, const std::string& message) override;
  void OnTimeout() override;

  // blink::mojom::SmsManager
  void GetNextMessage(base::TimeDelta timeout, GetNextMessageCallback) override;

 private:
  // Manages the queue of callbacks.
  void Push(GetNextMessageCallback);
  blink::mojom::SmsManager::GetNextMessageCallback Pop();

  // |sms_provider_| is safe because all instances of SmsManagerImpl are owned
  // by SmsServiceImpl through a StrongBindingSet.
  SmsProvider* sms_provider_;
  const url::Origin origin_;
  std::queue<GetNextMessageCallback> callbacks_;

  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(SmsManagerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMS_SMS_MANAGER_IMPL_H_
