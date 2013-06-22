// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/diagnostics/diagnostics_api.h"

namespace extensions {

void DiagnosticsSendPacketFunction::AsyncWorkStart() {
  OnCompleted(SEND_PACKET_NOT_IMPLEMENTED, "", 0.0);
}

}  // namespace extensions
