// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_HID_HID_SERVICE_H_
#define CONTENT_BROWSER_HID_HID_SERVICE_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "third_party/blink/public/mojom/hid/hid.mojom.h"

namespace content {

// HidService provides an implementation of the HidService mojom interface. This
// interface is used by Blink to implement the WebHID API.
class HidService : public blink::mojom::HidService {
 public:
  explicit HidService();
  ~HidService() override;

  void Bind(blink::mojom::HidServiceRequest request);

 private:
  mojo::BindingSet<blink::mojom::HidService> bindings_;

  DISALLOW_COPY_AND_ASSIGN(HidService);
};

}  // namespace content

#endif  // CONTENT_BROWSER_HID_HID_SERVICE_H_
