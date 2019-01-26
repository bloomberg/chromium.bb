// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FEATURES_H_
#define DEVICE_FIDO_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "build/build_config.h"

namespace device {

#if defined(OS_WIN)
COMPONENT_EXPORT(DEVICE_FIDO)
extern const base::Feature kWebAuthUseNativeWinApi;
#endif  // defined(OS_WIN)

// Controls the proxying of Cryptotoken requests through WebAuthn.
COMPONENT_EXPORT(DEVICE_FIDO)
extern const base::Feature kWebAuthProxyCryptotoken;

}  // namespace device

#endif  // DEVICE_FIDO_FEATURES_H_
