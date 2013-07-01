// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_COMPRESSION_UTILS_H_
#define CHROME_BROWSER_METRICS_COMPRESSION_UTILS_H_

#include <string>

namespace chrome {

// Compresses the text in |input| using gzip storing the result in |output|.
bool GzipCompress(const std::string& input, std::string* output);

}  // namespace chrome

#endif  // CHROME_BROWSER_METRICS_COMPRESSION_UTILS_H_
