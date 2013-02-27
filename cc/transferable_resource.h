// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TRANSFERABLE_RESOURCE_H_
#define CC_TRANSFERABLE_RESOURCE_H_

#include <vector>

#include "base/basictypes.h"
#include "cc/cc_export.h"
#include "ui/gfx/size.h"

namespace cc {

struct CC_EXPORT Mailbox {
  Mailbox();
  bool isZero() const;
  void setName(const int8* name);
  int8 name[64];
};

struct CC_EXPORT TransferableResource {
  TransferableResource();
  ~TransferableResource();

  unsigned id;
  unsigned sync_point;
  uint32 format;
  uint32 filter;
  gfx::Size size;
  Mailbox mailbox;
};

typedef std::vector<TransferableResource> TransferableResourceArray;

}  // namespace cc

#endif  // CC_TRANSFERABLE_RESOURCE_H_
