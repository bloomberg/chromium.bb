// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/hwid_checker.h"

#include <cstdio>

#include "base/chromeos/chromeos_version.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/system/statistics_provider.h"
#include "chrome/common/chrome_switches.h"
#include "third_party/re2/re2/re2.h"
#include "third_party/zlib/zlib.h"

namespace chromeos {

std::string CalculateHWIDChecksum(const std::string& data) {
  unsigned crc32_checksum = static_cast<unsigned>(crc32(
      0,
      reinterpret_cast<const Bytef*>(data.c_str()),
      data.length()));
  // We take four least significant decimal digits of CRC-32.
  char checksum[5];
  int snprintf_result =
      snprintf(checksum, 5, "%04u", crc32_checksum % 10000);
  LOG_ASSERT(snprintf_result == 4);
  return checksum;
}

bool IsHWIDCorrect(const std::string& hwid) {
  // TODO(dzhioev): add support of HWID v3 format.
  std::string body;
  std::string checksum;
  if (!RE2::FullMatch(hwid, "([\\s\\S]*) (\\d{4})", &body, &checksum))
    return false;
  return CalculateHWIDChecksum(body) == checksum;
}

bool IsMachineHWIDCorrect() {
#if !defined(GOOGLE_CHROME_BUILD)
  return true;
#endif
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType))
    return true;
  if (!base::chromeos::IsRunningOnChromeOS())
    return true;
  std::string hwid;
  chromeos::system::StatisticsProvider* stats =
      chromeos::system::StatisticsProvider::GetInstance();
  if (!stats->GetMachineStatistic("hardware_class", &hwid)) {
    LOG(ERROR) << "Couldn't get machine statistic 'hardware_class'.";
    return false;
  }
  if (!chromeos::IsHWIDCorrect(hwid)) {
    LOG(ERROR) << "Machine has malformed HWID '" << hwid << "'.";
    return false;
  }
  return true;
}

} // namespace chromeos

