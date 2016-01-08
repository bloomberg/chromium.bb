// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_BLIMP_STARTUP_H_
#define BLIMP_CLIENT_BLIMP_STARTUP_H_

#include "blimp/client/blimp_client_export.h"

namespace blimp {
namespace client {

BLIMP_CLIENT_EXPORT void InitializeLogging();

BLIMP_CLIENT_EXPORT bool InitializeMainMessageLoop();

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_BLIMP_STARTUP_H_
