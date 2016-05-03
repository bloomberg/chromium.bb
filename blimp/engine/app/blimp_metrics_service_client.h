// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_APP_BLIMP_METRICS_SERVICE_CLIENT_H_
#define BLIMP_ENGINE_APP_BLIMP_METRICS_SERVICE_CLIENT_H_

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"

class PrefService;

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace blimp {
namespace engine {

// Initializes metrics reporting for Blimp including initializing metrics
// providers and reporting state.
void InitializeBlimpMetrics(
    std::unique_ptr<PrefService> pref_service,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter);

// Graceful and expected metrics reporting shutdown for Blimp.
void FinalizeBlimpMetrics();

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_APP_BLIMP_METRICS_SERVICE_CLIENT_H_
