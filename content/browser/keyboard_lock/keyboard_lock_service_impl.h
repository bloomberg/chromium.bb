// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "third_party/WebKit/public/platform/modules/keyboard_lock/keyboard_lock.mojom.h"

namespace content {

class CONTENT_EXPORT KeyboardLockServiceImpl
    : public blink::mojom::KeyboardLockService {
 public:
  KeyboardLockServiceImpl();
  ~KeyboardLockServiceImpl() override;

  static void CreateMojoService(
      blink::mojom::KeyboardLockServiceRequest request);

  // blink::mojom::KeyboardLockService implementations.
  void RequestKeyboardLock(const std::vector<std::string>& key_codes,
                           RequestKeyboardLockCallback callback) override;
  void CancelKeyboardLock() override;
};

}  // namespace
