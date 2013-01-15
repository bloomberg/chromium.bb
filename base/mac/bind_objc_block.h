// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MAC_BIND_OBJC_BLOCK_H_
#define BASE_MAC_BIND_OBJC_BLOCK_H_

#include <Block.h>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/mac/scoped_block.h"

// BindBlock builds a callback from an Objective-C block. Example usages:
//
// Closure closure = BindBlock(^{DoSomething();});
// Callback<int(void)> callback = BindBlock(^{return 42;});

namespace base {

namespace internal {

// Helper function to run the block contained in the parameter.
template<typename ReturnType>
ReturnType RunBlock(base::mac::ScopedBlock<ReturnType(^)()> block) {
  ReturnType(^extracted_block)() = block.get();
  return extracted_block();
}

}  // namespace internal

// Construct a callback from an objective-C block.
template<typename ReturnType>
base::Callback<ReturnType(void)> BindBlock(ReturnType(^block)()) {
  return base::Bind(&base::internal::RunBlock<ReturnType>,
                    base::mac::ScopedBlock<ReturnType(^)()>(
                        Block_copy(block)));
}

}  // namespace base

#endif  // BASE_MAC_BIND_OBJC_BLOCK_H_
