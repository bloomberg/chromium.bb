// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_MANIFEST_H_
#define ASH_PUBLIC_CPP_TEST_MANIFEST_H_

#include "services/service_manager/public/cpp/manifest.h"

namespace ash {

// A manifest for the ash service with additional capabilities exposed for
// testing APIs. This can be amended to the normal manifest by passing it to
// ash::AmendManifestForTesting (in manifest.h) in a test environment before
// initializing the Service Manager, or by explicitly a mending a copy of the
// normal manifest (in e.g. unit tests using TestServiceManager).
const service_manager::Manifest& GetManifestOverlayForTesting();

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_MANIFEST_H_
