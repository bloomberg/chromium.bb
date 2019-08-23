// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_CERTIFICATE_TEST_HELPERS_H_
#define CAST_COMMON_CERTIFICATE_TEST_HELPERS_H_

#include <string>
#include <vector>

namespace cast {
namespace certificate {
namespace testing {

std::vector<std::string> ReadCertificatesFromPemFile(
    const std::string& filename);

}  // namespace testing
}  // namespace certificate
}  // namespace cast

#endif  // CAST_COMMON_CERTIFICATE_TEST_HELPERS_H_
