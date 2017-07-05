// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_STORED_PAYMENT_APP_H_
#define CONTENT_PUBLIC_BROWSER_STORED_PAYMENT_APP_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "content/common/content_export.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/origin.h"

namespace content {

// This class represents the stored payment app.
struct CONTENT_EXPORT StoredPaymentApp {
  StoredPaymentApp();
  ~StoredPaymentApp();

  // Id of the service worker registration this app is associated with.
  int64_t registration_id = 0;

  // Origin of the payment app provider that provides this payment app.
  url::Origin origin;

  // Label for this payment app.
  std::string name;

  // Decoded icon for this payment app.
  std::unique_ptr<SkBitmap> icon;

  // A list of one or more enabled payment methods in this payment app.
  std::vector<std::string> enabled_methods;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_STORED_PAYMENT_APP_H_