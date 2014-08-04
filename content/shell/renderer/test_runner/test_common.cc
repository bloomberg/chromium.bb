// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/test_common.h"

namespace content {

namespace {

const char layout_tests_pattern[] = "/LayoutTests/";
const std::string::size_type layout_tests_pattern_size =
    sizeof(layout_tests_pattern) - 1;
const char file_url_pattern[] = "file:/";
const char file_test_prefix[] = "(file test):";
const char data_url_pattern[] = "data:";
const std::string::size_type data_url_pattern_size =
    sizeof(data_url_pattern) - 1;

}  // namespace

std::string NormalizeLayoutTestURL(const std::string& url) {
  std::string result = url;
  size_t pos;
  if (!url.find(file_url_pattern) &&
      ((pos = url.find(layout_tests_pattern)) != std::string::npos)) {
    // adjust file URLs to match upstream results.
    result.replace(0, pos + layout_tests_pattern_size, file_test_prefix);
  } else if (!url.find(data_url_pattern)) {
    // URL-escape data URLs to match results upstream.
    std::string path = url.substr(data_url_pattern_size);
    result.replace(data_url_pattern_size, url.length(), path);
  }
  return result;
}

}  // namespace content
