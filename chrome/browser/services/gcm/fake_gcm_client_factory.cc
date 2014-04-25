// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/fake_gcm_client_factory.h"

#include "base/memory/scoped_ptr.h"
#include "google_apis/gcm/gcm_client.h"

namespace gcm {

FakeGCMClientFactory::FakeGCMClientFactory(
    GCMClientMock::LoadingDelay gcm_client_loading_delay)
    : gcm_client_loading_delay_(gcm_client_loading_delay) {
}

FakeGCMClientFactory::~FakeGCMClientFactory() {
}

scoped_ptr<GCMClient> FakeGCMClientFactory::BuildInstance() {
  return scoped_ptr<GCMClient>(new GCMClientMock(gcm_client_loading_delay_));
}

}  // namespace gcm
