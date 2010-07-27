// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/communicator/connection_options.h"

namespace notifier {

ConnectionOptions::ConnectionOptions()
    : autodetect_proxy_(true),
      proxy_port_(0),
      use_proxy_auth_(0),
      allow_unverified_certs_(false) {
}

}  // namespace notifier
