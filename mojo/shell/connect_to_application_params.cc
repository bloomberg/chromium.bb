// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/connect_to_application_params.h"

#include <utility>

#include "mojo/shell/application_instance.h"

namespace mojo {
namespace shell {

ConnectToApplicationParams::ConnectToApplicationParams() {}

ConnectToApplicationParams::~ConnectToApplicationParams() {}

void ConnectToApplicationParams::SetSource(ApplicationInstance* source) {
  if (!source) {
    source_ = Identity();
    return;
  }

  source_ = source->identity();
}

void ConnectToApplicationParams::SetTarget(const Identity& target) {
  target_ = target;
  target_url_request_ = URLRequest::New();
  target_url_request_->url = target_.url().spec();
}

void ConnectToApplicationParams::SetTargetURL(const GURL& target_url) {
  target_ = Identity(target_url, target_.qualifier(), target_.filter());
  target_url_request_ = URLRequest::New();
  target_url_request_->url = target_.url().spec();
}

void ConnectToApplicationParams::SetTargetURLRequest(URLRequestPtr request) {
  Identity target = request ? Identity(GURL(request->url), target_.qualifier(),
                                       target_.filter())
                            : Identity();
  SetTargetURLRequest(std::move(request), target);
}

void ConnectToApplicationParams::SetTargetURLRequest(URLRequestPtr request,
                                                     const Identity& target) {
  target_url_request_ = std::move(request);
  target_ = target;
}

}  // namespace shell
}  // namespace mojo
