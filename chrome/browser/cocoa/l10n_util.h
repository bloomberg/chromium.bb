// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/string16.h"

namespace cocoa_l10n_util {

// Compare function for -[NSArray sortedArrayUsingFunction:context:] that
// sorts the views in Y order bottom up. |context| is ignored.
NSInteger CompareFrameY(id view1, id view2, void* context);

// Helper for tweaking views. If view is a:
//   checkbox, radio group or label: it gets a forced wrap at current size
//   editable field: left as is
//   anything else: do  +[GTMUILocalizerAndLayoutTweaker sizeToFitView:]
NSSize WrapOrSizeToFit(NSView* view);

// Walks views in top-down order, wraps each to their current width, and moves
// the latter ones down to prevent overlaps. Returns the vertical delta in view
// coordinates.
CGFloat VerticallyReflowGroup(NSArray* views);

// Like |ReplaceStringPlaceholders(const string16&, const string16&, size_t*)|,
// but for a NSString formatString.
NSString* ReplaceNSStringPlaceholders(NSString* formatString,
                                      const string16& a,
                                      size_t* offset);

}  // namespace cocoa_l10n_util
