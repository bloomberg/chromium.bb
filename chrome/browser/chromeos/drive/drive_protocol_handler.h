// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_PROTOCOL_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_PROTOCOL_HANDLER_H_

#include "base/basictypes.h"
#include "net/url_request/url_request_job_factory.h"

namespace drive {

class DriveProtocolHandler : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  explicit DriveProtocolHandler(void* profile_id);
  virtual ~DriveProtocolHandler();

  // Creates URLRequestJobs for drive:// URLs.
  virtual net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE;

 private:
  // The profile for processing Drive accesses. Should not be NULL.
  void* profile_id_;

  DISALLOW_COPY_AND_ASSIGN(DriveProtocolHandler);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_PROTOCOL_HANDLER_H_
