// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_U2F_U2F_RETURN_CODE_H_
#define DEVICE_U2F_U2F_RETURN_CODE_H_

namespace device {

enum class U2fReturnCode : uint8_t {
  SUCCESS,
  FAILURE,
  INVALID_PARAMS,
  CONDITIONS_NOT_SATISFIED,
};

}  // namespace device

#endif  // DEVICE_U2F_U2F_RETURN_CODE_H_
