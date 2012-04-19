// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_PROTOCOL_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_PROTOCOL_HANDLER_H_
#pragma once

#include <string>

namespace net {
class URLRequest;
class URLRequestJob;
}

namespace gdata {

class GDataProtocolHandler {
 public:
  // Creates URLRequestJob-extended job to handle gdata:// requests.
  static net::URLRequestJob* CreateJob(net::URLRequest* request,
                                       const std::string& scheme);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_PROTOCOL_HANDLER_H_
