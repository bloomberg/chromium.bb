// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_service.h"

#include "base/macros.h"

namespace device {

class UsbServiceWin : public UsbService {
 public:
  explicit UsbServiceWin(
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);
  ~UsbServiceWin() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UsbServiceWin);
};

}  // namespace device
