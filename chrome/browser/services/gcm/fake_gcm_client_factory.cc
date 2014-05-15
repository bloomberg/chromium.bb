// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/fake_gcm_client_factory.h"

#include "base/memory/scoped_ptr.h"
#include "google_apis/gcm/gcm_client.h"

namespace gcm {

FakeGCMClientFactory::FakeGCMClientFactory(
    FakeGCMClient::StartMode gcm_client_start_mode)
    : gcm_client_start_mode_(gcm_client_start_mode) {
}

FakeGCMClientFactory::~FakeGCMClientFactory() {
}

scoped_ptr<GCMClient> FakeGCMClientFactory::BuildInstance() {
  return scoped_ptr<GCMClient>(new FakeGCMClient(gcm_client_start_mode_));
}

}  // namespace gcm
