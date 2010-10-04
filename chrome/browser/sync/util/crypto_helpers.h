// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_UTIL_CRYPTO_HELPERS_H_
#define CHROME_BROWSER_SYNC_UTIL_CRYPTO_HELPERS_H_
#pragma once

#include <string>
#include <vector>

// An object to handle calculation of MD5 sums.
#include "base/basictypes.h"
#include "base/md5.h"
#include "base/port.h"

class MD5Calculator {
 protected:
  MD5Context context_;
  std::vector<uint8> bin_digest_;

  void CalcDigest();
 public:
  MD5Calculator();
  ~MD5Calculator();
  void AddData(const uint8* data, int length);
  void AddData(const char* data, int length) {
    AddData(reinterpret_cast<const uint8*>(data), length);
  }
  std::string GetHexDigest();
  const std::vector<uint8>& GetDigest();
 private:
  DISALLOW_COPY_AND_ASSIGN(MD5Calculator);
};

void GetRandomBytes(char* output, int output_length);
std::string Generate128BitRandomHexString();

#endif  // CHROME_BROWSER_SYNC_UTIL_CRYPTO_HELPERS_H_
