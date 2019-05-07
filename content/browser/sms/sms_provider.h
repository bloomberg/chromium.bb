// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMS_SMS_PROVIDER_H_
#define CONTENT_BROWSER_SMS_SMS_PROVIDER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"

namespace content {

// This class wraps the platform-specific functions and allows tests to
// inject custom providers.
class SmsProvider {
 public:
  using SmsCallback =
      base::OnceCallback<void(bool, base::Optional<std::string>)>;

  SmsProvider() = default;
  virtual ~SmsProvider() = default;

  // Listen to the next incoming SMS and call the callback when it is received.
  // On timeout, call the callback with an empty message.
  virtual void Retrieve(base::TimeDelta timeout, SmsCallback callback) = 0;

  static std::unique_ptr<SmsProvider> Create();

 private:
  DISALLOW_COPY_AND_ASSIGN(SmsProvider);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMS_SMS_PROVIDER_H_
