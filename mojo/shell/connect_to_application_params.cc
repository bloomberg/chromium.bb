// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/connect_to_application_params.h"

#include "mojo/shell/application_instance.h"

namespace mojo {
namespace shell {

ConnectToApplicationParams::ConnectToApplicationParams() {}

ConnectToApplicationParams::~ConnectToApplicationParams() {}

void ConnectToApplicationParams::SetOriginatorInfo(
    ApplicationInstance* originator) {
  if (!originator) {
    originator_identity_ = Identity();
    originator_filter_.clear();
    return;
  }

  originator_identity_ = originator->identity();
  originator_filter_ = originator->filter();
}

void ConnectToApplicationParams::SetURLInfo(const GURL& app_url) {
  app_url_ = app_url;
  app_url_request_ = URLRequest::New();
  app_url_request_->url = app_url_.spec();
}

void ConnectToApplicationParams::SetURLInfo(URLRequestPtr app_url_request) {
  app_url_request_ = app_url_request.Pass();
  app_url_ = app_url_request_ ? GURL(app_url_request_->url) : GURL();
}

}  // namespace shell
}  // namespace mojo
