// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/view_manager/view_observer.h"

#include "base/basictypes.h"

namespace mojo {

////////////////////////////////////////////////////////////////////////////////
// ViewObserver, public:

ViewObserver::TreeChangeParams::TreeChangeParams()
    : target(NULL),
      old_parent(NULL),
      new_parent(NULL),
      receiver(NULL) {}

}  // namespace mojo
