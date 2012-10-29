// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/external_protocol/external_protocol_handler.h"

#include "base/logging.h"

// static
void ExternalProtocolHandler::RunExternalProtocolDialog(
    const GURL& url, int render_process_host_id, int routing_id) {
  // Chrome on Android uses a throttle-based mechansim to intercept links
  // so that the user may choose to run an Android application instead of
  // loading the link in the browser. The throttle is also used to handle
  // external protocols, so this code should not be reachable.
  NOTREACHED();
}
