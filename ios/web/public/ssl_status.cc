// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/ssl_status.h"

namespace web {

SSLStatus::SSLStatus()
    : security_style(SECURITY_STYLE_UNKNOWN),
      cert_status(0),
      connection_status(0),
      content_status(NORMAL_CONTENT) {
}

SSLStatus::SSLStatus(const SSLStatus& other) = default;

SSLStatus::~SSLStatus() {}

}  // namespace web
