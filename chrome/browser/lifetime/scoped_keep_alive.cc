// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/scoped_keep_alive.h"

#include "chrome/browser/lifetime/keep_alive_registry.h"
#include "chrome/browser/lifetime/keep_alive_types.h"

ScopedKeepAlive::ScopedKeepAlive(KeepAliveOrigin origin) : origin_(origin) {
  KeepAliveRegistry::GetInstance()->Register(origin_);
}

ScopedKeepAlive::~ScopedKeepAlive() {
  KeepAliveRegistry::GetInstance()->Unregister(origin_);
}
