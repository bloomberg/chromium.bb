// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_USER_AGENT_H_
#define IOS_WEB_PUBLIC_USER_AGENT_H_

#include <string>

namespace web {

// Returns the user agent to use for the given product name.
// The returned user agent is very similar to that used by Mobile Safari, for
// web page compatibility.
std::string BuildUserAgentFromProduct(const std::string& product);

}  // namespace web

#endif  // IOS_WEB_PUBLIC_USER_AGENT_H_
