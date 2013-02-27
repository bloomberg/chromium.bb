// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "cc/transferable_resource.h"

namespace cc {

Mailbox::Mailbox() {
  memset(name, 0, sizeof(name));
}

bool Mailbox::isZero() const {
  for (int i = 0; i < arraysize(name); ++i) {
    if (name[i])
      return false;
  }
  return true;
}

void Mailbox::setName(const int8* n) {
  DCHECK(isZero() || !memcmp(name, n, sizeof(name)));
  memcpy(name, n, sizeof(name));
}

TransferableResource::TransferableResource()
    : id(0),
      sync_point(0),
      format(0),
      filter(0) {
}

TransferableResource::~TransferableResource() {
}

}  // namespace cc
