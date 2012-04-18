// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BLUETOOTH_BLUETOOTH_SERVICE_RECORD_H_
#define CHROME_BROWSER_CHROMEOS_BLUETOOTH_BLUETOOTH_SERVICE_RECORD_H_
#pragma once

#include <string>

#include "base/basictypes.h"

namespace chromeos {

// The BluetoothServiceRecord represents an SDP service record.
//
// This implementation is currently incomplete: it only supports those fields
// that have been necessary so far.
class BluetoothServiceRecord {
 public:
   explicit BluetoothServiceRecord(const std::string& xml_data);

   const std::string& name() const { return name_; }

 private:
  std::string name_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothServiceRecord);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_BLUETOOTH_BLUETOOTH_SERVICE_RECORD_H_
