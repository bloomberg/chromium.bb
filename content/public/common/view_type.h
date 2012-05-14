// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_VIEW_TYPE_H_
#define CONTENT_PUBLIC_COMMON_VIEW_TYPE_H_
#pragma once

namespace content {

// Icky RTTI used by a few systems to distinguish the host type of a given
// RenderViewHost or WebContents.
//
// TODO(aa): Remove this and teach those systems to keep track of their own
// data.
enum ViewTypeValues {
  VIEW_TYPE_INVALID,
  VIEW_TYPE_INTERSTITIAL_PAGE,
  VIEW_TYPE_DEV_TOOLS_UI,
  VIEW_TYPE_CONTENT_END
};

typedef int ViewType;

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_VIEW_TYPE_H_
