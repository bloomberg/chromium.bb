// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_OS_CRYPT_FEATURES_MAC_H_
#define COMPONENTS_OS_CRYPT_OS_CRYPT_FEATURES_MAC_H_

#include "base/component_export.h"
#include "base/feature_list.h"

namespace os_crypt {
namespace features {

COMPONENT_EXPORT(OS_CRYPT)
extern const base::Feature kPreventEncryptionKeyOverwrites;

}  // namespace features
}  // namespace os_crypt

#endif  // COMPONENTS_OS_CRYPT_OS_CRYPT_FEATURES_MAC_H_
