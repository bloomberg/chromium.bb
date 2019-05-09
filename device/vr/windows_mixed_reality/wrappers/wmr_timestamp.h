// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_TIMESTAMP_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_TIMESTAMP_H_

#include <windows.perception.h>
#include <wrl.h>

#include "base/macros.h"

namespace device {
class WMRTimestamp {
 public:
  explicit WMRTimestamp(
      Microsoft::WRL::ComPtr<ABI::Windows::Perception::IPerceptionTimestamp>
          timestamp);
  virtual ~WMRTimestamp();

  virtual ABI::Windows::Foundation::DateTime TargetTime() const;
  virtual ABI::Windows::Foundation::TimeSpan PredictionAmount() const;
  ABI::Windows::Perception::IPerceptionTimestamp* GetRawPtr() const;

 protected:
  // Necessary so subclasses don't call the explicit constructor.
  WMRTimestamp();

 private:
  Microsoft::WRL::ComPtr<ABI::Windows::Perception::IPerceptionTimestamp>
      timestamp_;

  DISALLOW_COPY_AND_ASSIGN(WMRTimestamp);
};
}  // namespace device
#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_TIMESTAMP_H_
