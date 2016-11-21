// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/passwords/password_generation_utils.h"

#include "base/i18n/rtl.h"
#include "ios/chrome/browser/ui/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace passwords {

namespace {

const CGFloat kPadding = IsIPadIdiom() ? 16 : 8;

// The actual implementation of |RunPipeline| that begins with the first block
// in |blocks|.
void RunSearchPipeline(NSArray* blocks,
                       PipelineCompletionBlock on_complete,
                       NSUInteger from_index) {
  if (from_index == [blocks count]) {
    on_complete(NSNotFound);
    return;
  }
  PipelineBlock block = blocks[from_index];
  block(^(BOOL success) {
    if (success)
      on_complete(from_index);
    else
      RunSearchPipeline(blocks, on_complete, from_index + 1);
  });
}

}  // namespace

CGRect GetGenerationAccessoryFrame(CGRect outer_frame, CGRect inner_frame) {
  CGFloat x = kPadding;
  if (base::i18n::IsRTL())
    x = CGRectGetWidth(outer_frame) - CGRectGetWidth(inner_frame) - kPadding;
  const CGFloat y =
      (CGRectGetHeight(outer_frame) - CGRectGetHeight(inner_frame)) / 2.0;
  inner_frame.origin = CGPointMake(x, y);
  return inner_frame;
}

void RunSearchPipeline(NSArray* blocks, PipelineCompletionBlock on_complete) {
  RunSearchPipeline(blocks, on_complete, 0);
}

}  // namespace passwords
