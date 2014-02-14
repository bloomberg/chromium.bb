// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_SERIAL_SERIAL_DEVICE_INFO_H_
#define DEVICE_SERIAL_SERIAL_DEVICE_INFO_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"

namespace device {

struct SerialDeviceInfo {
  SerialDeviceInfo();
  ~SerialDeviceInfo();

  std::string path;
  scoped_ptr<uint16> vendor_id;
  scoped_ptr<uint16> product_id;
  scoped_ptr<std::string> display_name;

 private:
  DISALLOW_COPY_AND_ASSIGN(SerialDeviceInfo);
};

typedef std::vector<linked_ptr<SerialDeviceInfo> > SerialDeviceInfoList;

}  // namespace device

#endif  // DEVICE_SERIAL_SERIAL_DEVICE_INFO_H_
