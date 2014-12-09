// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_POWER_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_POWER_HANDLER_H_

#include "content/browser/devtools/protocol/devtools_protocol_handler.h"
#include "content/browser/power_profiler/power_profiler_observer.h"

namespace content {
namespace devtools {
namespace power {

class PowerHandler : public PowerProfilerObserver {
 public:
  typedef DevToolsProtocolClient::Response Response;

  PowerHandler();
  ~PowerHandler() override;

  void SetClient(scoped_ptr<Client> client);

  // PowerProfilerObserver override.
  void OnPowerEvent(const PowerEventVector& events) override;

  void Detached();

  Response Start();
  Response End();
  Response CanProfilePower(bool* result);
  Response GetAccuracyLevel(std::string* result);

 private:
  scoped_ptr<Client> client_;
  bool enabled_;

  DISALLOW_COPY_AND_ASSIGN(PowerHandler);
};

}  // namespace power
}  // namespace devtools
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_POWER_HANDLER_H_
