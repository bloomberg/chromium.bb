// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_BATTOR_POWER_TRACE_PROVIDER_H_
#define CONTENT_BROWSER_TRACING_BATTOR_POWER_TRACE_PROVIDER_H_

#include <string>

#include "base/macros.h"

namespace content {

// This class handles the connection with the battor h/w using
// chrome serial port interfaces.
// TODO(charliea): port over battor C code.
class BattorPowerTraceProvider {
 public:
  BattorPowerTraceProvider();
  bool IsConnected();
  bool StartTracing();
  bool StopTracing();
  void RecordClockSyncMarker(int sync_id);
  void GetLog(std::string* log_str);
  ~BattorPowerTraceProvider();

 private:
  DISALLOW_COPY_AND_ASSIGN(BattorPowerTraceProvider);
};
}
#endif  // CONTENT_BROWSER_TRACING_BATTOR_POWER_TRACE_PROVIDER_H_
