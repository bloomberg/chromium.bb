// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/common/config.h"
#include "discovery/mdns/mdns_reader.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  openscreen::discovery::Config config;
  openscreen::discovery::MdnsReader reader(config, data, size);
  openscreen::discovery::MdnsMessage message;
  reader.Read(&message);
  return 0;
}
