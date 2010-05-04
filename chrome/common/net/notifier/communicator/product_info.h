// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NET_NOTIFIER_COMMUNICATOR_PRODUCT_INFO_H_
#define CHROME_COMMON_NET_NOTIFIER_COMMUNICATOR_PRODUCT_INFO_H_

#include <string>

namespace notifier {
std::string GetUserAgentString();
std::string GetProductSignature();
}  // namespace notifier

#endif  // CHROME_COMMON_NET_NOTIFIER_COMMUNICATOR_PRODUCT_INFO_H_
