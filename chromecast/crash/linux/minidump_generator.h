// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CRASH_LINUX_MINIDUMP_GENERATOR_H_
#define CHROMECAST_CRASH_LINUX_MINIDUMP_GENERATOR_H_

#include <string>

namespace chromecast {

class MinidumpGenerator {
 public:
  virtual ~MinidumpGenerator() {}

  // Interface to generate a minidump file in given path.
  // This is called inside MinidumpWriter::DoWorkLocked().
  virtual bool Generate(const std::string& minidump_path) = 0;
};

}  // namespace chromecast

#endif  // CHROMECAST_CRASH_LINUX_MINIDUMP_GENERATOR_H_
