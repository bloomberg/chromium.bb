// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/ui/apps_client.h"

#include "base/basictypes.h"

namespace apps {

namespace {

AppsClient* g_client = NULL;

}  // namespace

AppsClient* AppsClient::Get() {
  return g_client;
}

void AppsClient::Set(AppsClient* client) {
  // This can happen in unit tests, where the utility thread runs in-process.
  if (g_client)
    return;

  g_client = client;
}

}  // namespace apps
