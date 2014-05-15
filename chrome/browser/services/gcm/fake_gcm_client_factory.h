// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICES_GCM_FAKE_GCM_CLIENT_FACTORY_H_
#define CHROME_BROWSER_SERVICES_GCM_FAKE_GCM_CLIENT_FACTORY_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/services/gcm/fake_gcm_client.h"
#include "components/gcm_driver/gcm_client_factory.h"

namespace gcm {

class GCMClient;

class FakeGCMClientFactory : public GCMClientFactory {
 public:
  explicit FakeGCMClientFactory(FakeGCMClient::StartMode gcm_client_start_mode);
  virtual ~FakeGCMClientFactory();

  // GCMClientFactory:
  virtual scoped_ptr<GCMClient> BuildInstance() OVERRIDE;

 private:
  FakeGCMClient::StartMode gcm_client_start_mode_;

  DISALLOW_COPY_AND_ASSIGN(FakeGCMClientFactory);
};

}  // namespace gcm

#endif  // CHROME_BROWSER_SERVICES_GCM_FAKE_GCM_CLIENT_FACTORY_H_
