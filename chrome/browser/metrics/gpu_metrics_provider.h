// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_GPU_METRICS_PROVIDER_H_
#define CHROME_BROWSER_METRICS_GPU_METRICS_PROVIDER_H_

#include "base/basictypes.h"
#include "components/metrics/metrics_provider.h"
#include "ui/gfx/size.h"

// GPUMetricsProvider provides GPU-related metrics.
class GPUMetricsProvider : public metrics::MetricsProvider {
 public:
  GPUMetricsProvider();
  virtual ~GPUMetricsProvider();

  // metrics::MetricsProvider:
  virtual void ProvideSystemProfileMetrics(
      metrics::SystemProfileProto* system_profile_proto) OVERRIDE;

 protected:
  // Exposed for the sake of mocking in test code.

  // Returns the screen size for the primary monitor.
  virtual gfx::Size GetScreenSize() const;

  // Returns the device scale factor for the primary monitor.
  virtual float GetScreenDeviceScaleFactor() const;

  // Returns the number of monitors the user is using.
  virtual int GetScreenCount() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(GPUMetricsProvider);
};

#endif  // CHROME_BROWSER_METRICS_GPU_METRICS_PROVIDER_H_
