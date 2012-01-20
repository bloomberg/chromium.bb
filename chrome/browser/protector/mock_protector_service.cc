// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/protector/mock_protector_service.h"

#include "chrome/browser/protector/protector_service_factory.h"
#include "googleurl/src/gurl.h"

namespace protector {

namespace {

ProfileKeyedService* BuildMockProtectorService(Profile* profile) {
  return new MockProtectorService(profile);
}

}  // namespace

// static
MockProtectorService* MockProtectorService::BuildForProfile(Profile* profile) {
  return static_cast<MockProtectorService*>(
      ProtectorServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile, BuildMockProtectorService));
}

MockProtectorService::MockProtectorService(Profile* profile)
    : ProtectorService(profile) {
}

MockProtectorService::~MockProtectorService() {
}

}  // namespace protector
