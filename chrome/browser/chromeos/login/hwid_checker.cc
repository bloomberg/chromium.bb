// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/hwid_checker.h"

#include <cstdio>

#include "base/chromeos/chromeos_version.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/system/statistics_provider.h"
#include "chrome/common/chrome_switches.h"
#include "third_party/re2/re2/re2.h"
#include "third_party/zlib/zlib.h"

namespace {

unsigned CalculateCRC32(const std::string& data) {
  return static_cast<unsigned>(crc32(
      0,
      reinterpret_cast<const Bytef*>(data.c_str()),
      data.length()));
}

std::string CalculateHWIDv2Checksum(const std::string& data) {
  unsigned crc32 = CalculateCRC32(data);
  // We take four least significant decimal digits of CRC-32.
  char checksum[5];
  int snprintf_result =
      snprintf(checksum, 5, "%04u", crc32 % 10000);
  LOG_ASSERT(snprintf_result == 4);
  return checksum;
}

bool IsCorrectHWIDv2(const std::string& hwid) {
  std::string body;
  std::string checksum;
  if (!RE2::FullMatch(hwid, "([\\s\\S]*) (\\d{4})", &body, &checksum))
    return false;
  return CalculateHWIDv2Checksum(body) == checksum;
}

std::string CalculateHWIDv3Checksum(const std::string& data) {
  static const char base32_alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
  unsigned crc32 = CalculateCRC32(data);
  // We take 10 least significant bits of CRC-32 and encode them in 2 characters
  // using Base32 alphabet.
  std::string checksum;
  checksum += base32_alphabet[(crc32 >> 5) & 0x1f];
  checksum += base32_alphabet[crc32 & 0x1f];
  return checksum;
}

bool IsCorrectHWIDv3(const std::string& hwid) {
  std::string bom;
  if (!RE2::FullMatch(hwid, "[A-Z0-9]+ ((?:[A-Z2-7]{4}-)*[A-Z2-7]{1,4})", &bom))
    return false;
  if (bom.length() < 2)
    return false;
  std::string hwid_without_dashes;
  RemoveChars(hwid, "-", &hwid_without_dashes);
  LOG_ASSERT(hwid_without_dashes.length() >= 2);
  std::string not_checksum =
      hwid_without_dashes.substr(0, hwid_without_dashes.length() - 2);
  std::string checksum =
      hwid_without_dashes.substr(hwid_without_dashes.length() - 2);
  return CalculateHWIDv3Checksum(not_checksum) == checksum;
}

} // anonymous namespace

namespace chromeos {

bool IsHWIDCorrect(const std::string& hwid) {
  return IsCorrectHWIDv2(hwid) || IsCorrectHWIDv3(hwid);
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

