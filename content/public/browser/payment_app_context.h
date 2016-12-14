// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PAYMENT_APP_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_PAYMENT_APP_CONTEXT_H_

#include <utility>
#include <vector>

#include "base/callback_forward.h"

namespace content {

class PaymentAppContext {
 public:
  virtual ~PaymentAppContext() {}

  // The ManifestWithID is a pair of the service worker registration id and
  // the payment app manifest data associated with it.
  using ManifestWithID =
      std::pair<int64_t, payments::mojom::PaymentAppManifestPtr>;
  using Manifests = std::vector<ManifestWithID>;
  using GetAllManifestsCallback = base::Callback<void(Manifests)>;

  virtual void GetAllManifests(const GetAllManifestsCallback& callback) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PAYMENT_APP_CONTEXT_H_
