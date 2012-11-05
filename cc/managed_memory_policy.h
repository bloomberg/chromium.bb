// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_MANAGED_MEMORY_POLICY_H_
#define CC_MANAGED_MEMORY_POLICY_H_

#include "base/basictypes.h"
#include "cc/cc_export.h"

namespace cc {

struct CC_EXPORT ManagedMemoryPolicy {
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

#endif  // CC_MANAGED_MEMORY_POLICY_H_
