// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRACING_STARTUP_TRACING_H_
#define COMPONENTS_TRACING_STARTUP_TRACING_H_

#include "components/tracing/tracing_export.h"

namespace tracing {

// Enable startup tracing according to the trace config file. If the trace
// config file does not exist, it will do nothing. This is designed to be used
// by Telemetry. Telemetry will stop tracing via DevTools later. To avoid
// conflict, this should not be used when --trace-startup is enabled.
void TRACING_EXPORT EnableStartupTracingIfConfigFileExists();

}  // namespace tracing

#endif  // COMPONENTS_TRACING_STARTUP_TRACING_H_
