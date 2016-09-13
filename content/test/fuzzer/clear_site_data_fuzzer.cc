// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <vector>

#include "content/browser/browsing_data/clear_site_data_throttle.h"

namespace content {

class ClearSiteDataFuzzerTest {
 public:
  ClearSiteDataFuzzerTest() : throttle_(nullptr) {}

  void TestHeader(const std::string& header) {
    bool remove_cookies;
    bool remove_storage;
    bool remove_cache;
    std::vector<content::ClearSiteDataThrottle::ConsoleMessage> messages;

    throttle_.ParseHeader(header, &remove_cookies, &remove_storage,
                          &remove_cache, &messages);
  }

 private:
  content::ClearSiteDataThrottle throttle_;
};

ClearSiteDataFuzzerTest* test = new ClearSiteDataFuzzerTest();

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  test->TestHeader(std::string(reinterpret_cast<const char*>(data), size));
  return 0;
}

}  // namespace content
