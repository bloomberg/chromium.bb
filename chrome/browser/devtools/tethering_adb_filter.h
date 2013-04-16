// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_TETHERING_ADB_FILTER_H_
#define CHROME_BROWSER_DEVTOOLS_TETHERING_ADB_FILTER_H_

#include <map>
#include <string>

#include "base/basictypes.h"

class TetheringAdbFilter {
 public:
  TetheringAdbFilter(int adb_port, const std::string& serial);
  ~TetheringAdbFilter();

  bool ProcessIncomingMessage(const std::string& message);
  void ProcessOutgoingMessage(const std::string& message);

 private:
  int adb_port_;
  std::string serial_;
  std::map<int, std::string> requested_ports_;
  DISALLOW_COPY_AND_ASSIGN(TetheringAdbFilter);
};

#endif  // CHROME_BROWSER_DEVTOOLS_TETHERING_ADB_FILTER_H_
