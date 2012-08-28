// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CONTENT_CONSTANTS_INTERNAL_H_
#define CONTENT_COMMON_CONTENT_CONSTANTS_INTERNAL_H_

namespace content {

// Keys used for serializing the frame tree of a renderer process, used for
// ViewMsg_UpdateFrameTree and ViewHostMsg_FrameTreeUpdated.
extern const char kFrameTreeNodeNameKey[];
extern const char kFrameTreeNodeIdKey[];
extern const char kFrameTreeNodeSubtreeKey[];

} // namespace content

#endif  // CONTENT_COMMON_CONTENT_CONSTANTS_INTERNAL_H_
