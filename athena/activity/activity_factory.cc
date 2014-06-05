// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/activity/public/activity_factory.h"

#include "base/logging.h"

namespace athena {

namespace {

ActivityFactory* instance = NULL;

}

// static
void ActivityFactory::RegisterActivityFactory(ActivityFactory* factory) {
  DCHECK(!instance);
  instance = factory;
}

// static
ActivityFactory* ActivityFactory::Get() {
  DCHECK(instance);
  return instance;
}

// static
void ActivityFactory::Shutdown() {
  DCHECK(instance);
  delete instance;
  instance = NULL;
}

}  // namespace athena
