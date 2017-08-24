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
#include "url/gurl.h"

namespace content {

// This class represents the stored related application of the StoredPaymentApp.
struct CONTENT_EXPORT StoredRelatedApplication {
  StoredRelatedApplication();
  ~StoredRelatedApplication();

  std::string platform;
  std::string id;
};

// This class represents the stored payment app.
struct CONTENT_EXPORT StoredPaymentApp {
  StoredPaymentApp();
  ~StoredPaymentApp();

  // Id of the service worker registration this app is associated with.
  int64_t registration_id = 0;

  // Scope of the service worker that implements this payment app.
  GURL scope;

  // Label for this payment app.
  std::string name;

  // Decoded icon for this payment app.
  std::unique_ptr<SkBitmap> icon;

  // A list of one or more enabled payment methods in this payment app.
  std::vector<std::string> enabled_methods;

  // A flag indicates whether the app prefers the related applications.
  bool prefer_related_applications = false;

  // A list of stored related applications.
  std::vector<StoredRelatedApplication> related_applications;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_STORED_PAYMENT_APP_H_