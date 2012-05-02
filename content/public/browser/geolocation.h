// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_GEOLOCATION_H_
#define CONTENT_PUBLIC_BROWSER_GEOLOCATION_H_
#pragma once

#include "base/callback_forward.h"
#include "content/common/content_export.h"

namespace content {

struct Geoposition;

// Overrides the current location for testing. This function may be called on
// any thread. The completion callback will be invoked asynchronously on the
// calling thread when the override operation is completed.
// This should be used instead of a mock location provider for a simpler way
// to provide fake location results when not testing the innards of the
// geolocation code.
void CONTENT_EXPORT OverrideLocationForTesting(
    const Geoposition& position,
    const base::Closure& completion_callback);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_GEOLOCATION_H_
