// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_MAC_FAKE_TOUCH_ID_CONTEXT_H_
#define DEVICE_FIDO_MAC_FAKE_TOUCH_ID_CONTEXT_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "device/fido/mac/touch_id_context.h"

namespace device {
namespace fido {
namespace mac {

class API_AVAILABLE(macosx(10.12.2)) FakeTouchIdContext
    : public TouchIdContext {
 public:
  ~FakeTouchIdContext() override;

  // TouchIdContext:
  void PromptTouchId(const base::string16& reason, Callback callback) override;

  void set_callback_result(bool callback_result) {
    callback_result_ = callback_result;
  }

 private:
  friend class ScopedTouchIdTestEnvironment;

  FakeTouchIdContext();

  bool callback_result_ = true;

  DISALLOW_COPY_AND_ASSIGN(FakeTouchIdContext);
};

}  // namespace mac
}  // namespace fido
}  // namespace device

#endif  // DEVICE_FIDO_MAC_FAKE_TOUCH_ID_CONTEXT_H_
