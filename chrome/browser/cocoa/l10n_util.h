// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

namespace cocoa_l10n_util {

// Compare function for -[NSArray sortedArrayUsingFunction:context:] that
// sorts the views in Y order bottom up. |context| is ignored.
NSInteger CompareFrameY(id view1, id view2, void* context);

// Helper for tweaking views. If view is a:
//   checkbox, radio group or label: it gets a forced wrap at current size
//   editable field: left as is
//   anything else: do  +[GTMUILocalizerAndLayoutTweaker sizeToFitView:]
NSSize WrapOrSizeToFit(NSView* view);

}  // namespace cocoa_l10n_util
