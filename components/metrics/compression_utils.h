// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_COMPRESSION_UTILS_H_
#define COMPONENTS_METRICS_COMPRESSION_UTILS_H_

#include <string>

namespace metrics {

// Compresses the data in |input| using gzip, storing the result in |output|.
bool GzipCompress(const std::string& input, std::string* output);

// Uncompresses the data in |input| using gzip, storing the result in |output|.
bool GzipUncompress(const std::string& input, std::string* output);

}  // namespace metrics

#endif  // COMPONENTS_METRICS_COMPRESSION_UTILS_H_
