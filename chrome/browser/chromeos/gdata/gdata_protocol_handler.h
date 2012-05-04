// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_PROTOCOL_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_PROTOCOL_HANDLER_H_
#pragma once

#include "net/url_request/url_request_job_factory.h"

namespace gdata {

class GDataProtocolHandler : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  GDataProtocolHandler();
  virtual ~GDataProtocolHandler();
  // Creates URLRequestJobs for drive:// URLs.
  virtual net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request) const OVERRIDE;
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_PROTOCOL_HANDLER_H_
