// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_IOS_TEST_TEST_SERVER_H_
#define COMPONENTS_CRONET_IOS_TEST_TEST_SERVER_H_

#include <string>

namespace cronet {

class TestServer {
 public:
  static bool Start();
  static void Shutdown();

  // Returns URL which respond with echo of header with |header_name| in
  // response body.
  static std::string GetEchoHeaderURL(const std::string& header_name);
  // Returns URL which respond with setting cookie to |cookie_line| and echo it
  // in response body.
  static std::string GetSetCookieURL(const std::string& cookie_line);
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_IOS_TEST_TEST_SERVER_H_
