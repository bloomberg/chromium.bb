// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/privetv3_session.h"

#include "base/logging.h"
#include "chrome/browser/local_discovery/privet_http.h"

namespace local_discovery {

PrivetV3Session::Delegate::~Delegate() {
}

PrivetV3Session::Request::~Request() {
}

PrivetV3Session::PrivetV3Session(scoped_ptr<PrivetHTTPClient> client,
                                 Delegate* delegate) {
}

PrivetV3Session::~PrivetV3Session() {
}

void PrivetV3Session::Start() {
}

}  // namespace local_discovery
