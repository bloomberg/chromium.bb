// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_SERVICE_COMMON_H_
#define FUCHSIA_SERVICE_COMMON_H_

#include <zircon/processargs.h>

#include "fuchsia/common/fuchsia_export.h"

namespace webrunner {

// Switch passed to content process when running in incognito mode, i.e. when
// there is no kWebContextDataPath.
FUCHSIA_EXPORT extern const char kIncognitoSwitch[];

// This file contains constants and functions shared between Context and
// ContextProvider processes.

// Handle ID for the Context interface request passed from ContextProvider to
// Context process.
constexpr uint32_t kContextRequestHandleId = PA_HND(PA_USER0, 0);

}  // namespace webrunner

#endif  // FUCHSIA_SERVICE_COMMON_H_
