// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/content/browser/content_lofi_decider.h"

namespace data_reduction_proxy {

ContentLoFiDecider::ContentLoFiDecider() {}

ContentLoFiDecider::~ContentLoFiDecider() {}

bool ContentLoFiDecider::IsUsingLoFiMode(const net::URLRequest& request) const {
  // TODO: will be check of ResourceRequestInfo::IsUsingLoFi
  return false;
}

}  // namespace data_reduction_proxy
