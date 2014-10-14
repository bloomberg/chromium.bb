// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/routing_id_mangling_disabler.h"

#include "content/browser/renderer_host/routing_id_issuer.h"

namespace content {

RoutingIDManglingDisabler::RoutingIDManglingDisabler() {
  RoutingIDIssuer::DisableIDMangling();
}

RoutingIDManglingDisabler::~RoutingIDManglingDisabler() {
  RoutingIDIssuer::EnableIDMangling();
}

}  // namespace content
