// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/privet_http.h"

#include <utility>

#include "chrome/browser/printing/cloud_print/privet_http_impl.h"

namespace cloud_print {

// static
scoped_ptr<PrivetV1HTTPClient> PrivetV1HTTPClient::CreateDefault(
    scoped_ptr<PrivetHTTPClient> info_client) {
  if (!info_client)
    return scoped_ptr<PrivetV1HTTPClient>();
  return make_scoped_ptr<PrivetV1HTTPClient>(
      new PrivetV1HTTPClientImpl(std::move(info_client)));
}

}  // namespace cloud_print
