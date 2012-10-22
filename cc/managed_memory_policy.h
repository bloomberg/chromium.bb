// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef managed_memory_policy_h
#define managed_memory_policy_h

#include "base/basictypes.h"

namespace cc {

struct ManagedMemoryPolicy {
    ManagedMemoryPolicy(size_t bytesLimitWhenVisible);
    ManagedMemoryPolicy(size_t bytesLimitWhenVisible,
                        int priorityCutoffWhenVisible,
                        size_t bytesLimitWhenNotVisible,
                        int priorityCutoffWhenNotVisible);
    bool operator==(const ManagedMemoryPolicy&) const;
    bool operator!=(const ManagedMemoryPolicy&) const;

    size_t bytesLimitWhenVisible;
    int priorityCutoffWhenVisible;
    size_t bytesLimitWhenNotVisible;
    int priorityCutoffWhenNotVisible;
};

}  // namespace cc

#endif
