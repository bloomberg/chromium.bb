// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ACCELERATED_WINDOW_INTERFACE_H_
#define CONTENT_PUBLIC_BROWSER_ACCELERATED_WINDOW_INTERFACE_H_
#pragma once

// Protocol implemented by windows that need to be informed explicitly about
// underlay surfaces.
@protocol UnderlayableSurface
- (void)underlaySurfaceAdded;
- (void)underlaySurfaceRemoved;
@end

#endif  // CONTENT_PUBLIC_BROWSER_ACCELERATED_WINDOW_INTERFACE_H_
