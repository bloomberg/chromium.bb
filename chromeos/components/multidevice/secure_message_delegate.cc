// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/multidevice/secure_message_delegate.h"

namespace chromeos {

namespace multidevice {

SecureMessageDelegate::SecureMessageDelegate() = default;

SecureMessageDelegate::~SecureMessageDelegate() = default;

SecureMessageDelegate::CreateOptions::CreateOptions() = default;

SecureMessageDelegate::CreateOptions::CreateOptions(
    const CreateOptions& other) = default;

SecureMessageDelegate::CreateOptions::~CreateOptions() = default;

SecureMessageDelegate::UnwrapOptions::UnwrapOptions() = default;

SecureMessageDelegate::UnwrapOptions::~UnwrapOptions() = default;

}  // namespace multidevice

}  // namespace chromeos
