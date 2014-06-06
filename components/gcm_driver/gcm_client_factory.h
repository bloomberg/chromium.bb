// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_GCM_CLIENT_FACTORY_H_
#define COMPONENTS_GCM_DRIVER_GCM_CLIENT_FACTORY_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

namespace gcm {

class GCMClient;

class GCMClientFactory {
 public:
  GCMClientFactory();
  virtual ~GCMClientFactory();

  // Creates a new instance of GCMClient. The testing code could override this
  // to provide a mocked instance.
  virtual scoped_ptr<GCMClient> BuildInstance();

 private:
  DISALLOW_COPY_AND_ASSIGN(GCMClientFactory);
};

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_GCM_CLIENT_FACTORY_H_
