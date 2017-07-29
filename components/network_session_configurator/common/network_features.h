// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NETWORK_SESSION_CONFIGURATOR_COMMON_NETWORK_FEATURES_H_
#define COMPONENTS_NETWORK_SESSION_CONFIGURATOR_COMMON_NETWORK_FEATURES_H_

#include "base/feature_list.h"

namespace features {

// Enables token binding
// (https://www.ietf.org/id/draft-ietf-tokbind-protocol-04.txt).
extern const base::Feature kTokenBinding;

}  // namespace features

#endif  // COMPONENTS_NETWORK_SESSION_CONFIGURATOR_COMMON_NETWORK_FEATURES_H_
