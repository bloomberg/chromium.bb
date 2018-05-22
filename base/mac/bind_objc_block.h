// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MAC_BIND_OBJC_BLOCK_H_
#define BASE_MAC_BIND_OBJC_BLOCK_H_

#include <Block.h>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/mac/scoped_block.h"

namespace base {

#if !HAS_FEATURE(objc_arc)

// Construct a callback from an Objective-C block when ARC is disabled.
// Deprecated, instead use base::RetainBlock() and use base::BindOnce or
// base::BindRepeating with the wrapped block.
//
// For example, the following are equivalent:
//
//    // Deprecated.
//    base::RepeatingClosure closure =
//        base::BindRepeating(
//            base::BindBlock(^(int n) { NSLog(@"%d"); }),
//            1);
//
//    // Recommended.
//    base::RepeatingClosure closure =
//        base::BindRepeating(
//            base::RetainBlock(^(int n) { NSLog(@"%d"); }),
//            1);
template <typename R, typename... Args, typename... BoundArgs>
base::RepeatingCallback<
    MakeUnboundRunType<base::mac::ScopedBlock<R (^)(Args...)>, BoundArgs...>>
BindBlock(R (^block)(Args...), BoundArgs&&... args) {
  return base::BindRepeating(base::RetainBlock(block),
                             std::forward<BoundArgs>(args)...);
}

#else

// Construct a callback from an Objective-C block when ARC is enabled.
// Deprecated, instead use base::BindOnce or base::BindRepeating directly.
//
// For example, the following are equivalent:
//
//    // Deprecated.
//    base::RepeatingClosure closure =
//        base::BindRepeating(
//            base::BindBlockArc(^(int n) { NSLog(@"%d"); }),
//            1);
//
//    // Recommended.
//    base::RepeatingClosure closure =
//        base::BindRepeating(^(int n) { NSLog(@"%d"); }, 1);
template <typename R, typename... Args, typename... BoundArgs>
base::RepeatingCallback<MakeUnboundRunType<R (^)(Args...), BoundArgs...>>
BindBlockArc(R (^block)(Args...), BoundArgs&&... args) {
  return base::BindRepeating(block, std::forward<BoundArgs>(args)...);
}

#endif

}  // namespace base

#endif  // BASE_MAC_BIND_OBJC_BLOCK_H_
