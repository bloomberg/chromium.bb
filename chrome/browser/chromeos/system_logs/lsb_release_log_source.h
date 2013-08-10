// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_LOGS_LSB_RELEASE_LOG_SOURCE_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_LOGS_LSB_RELEASE_LOG_SOURCE_H_

#include "chrome/browser/chromeos/system_logs/system_logs_fetcher_base.h"

namespace chromeos {

// Fetches release information form /etc/lsb-release file.
class LsbReleaseLogSource : public SystemLogsSource {
 public:
  LsbReleaseLogSource() {}
  virtual ~LsbReleaseLogSource() {}

  // SystemLogsSource override.
  virtual void Fetch(const SysLogsSourceCallback& callback) OVERRIDE;

  // For testing. Parses a key=value pairs from a string. There can be only one
  // pair per line and the value must confine to a single line.
  // Example:
  // foo = 1
  // bar = 2
  // Note: it automatically strips whitespaces from the front and back of values
  // and keys.
  static void ParseLSBRelease(const std::string& lsb_data,
                              SystemLogsResponse* response);

 private:
  // Gets the info from the /etc/lsb-release and parses it
  static void ReadLSBRelease(SystemLogsResponse* response);

  DISALLOW_COPY_AND_ASSIGN(LsbReleaseLogSource);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_LOGS_LSB_RELEASE_LOG_SOURCE_H_
