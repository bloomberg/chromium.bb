// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_orientation/provider.h"

namespace device_orientation {

Provider* Provider::GetInstance() {
  if (!instance_)
    // TODO(hans) This is not finished. We will create an instance of the real
    // Provider implementation once it is implemented.
    instance_ = new Provider();
  return instance_;
}

void Provider::SetInstanceForTests(Provider* provider) {
  DCHECK(!instance_);
  instance_ = provider;
}

Provider* Provider::instance_ = NULL;

} //  namespace device_orientation
