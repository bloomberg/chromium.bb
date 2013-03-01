// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_protocol_handler.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/drive/drive_url_request_job.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request.h"

namespace drive {

DriveProtocolHandler::DriveProtocolHandler(void* profile_id)
  : profile_id_(profile_id) {
}

DriveProtocolHandler::~DriveProtocolHandler() {
}

net::URLRequestJob* DriveProtocolHandler::MaybeCreateJob(
    net::URLRequest* request, net::NetworkDelegate* network_delegate) const {
  DVLOG(1) << "Handling url: " << request->url().spec();
  return new DriveURLRequestJob(profile_id_, request, network_delegate);
}

}  // namespace drive
