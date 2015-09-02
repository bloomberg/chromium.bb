// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_VIEW_WEB_VIEW_SWITCHES_H_
#define COMPONENTS_WEB_VIEW_WEB_VIEW_SWITCHES_H_

namespace web_view {
namespace switches {

// If true a new HTMLFrameTreeManager is always created, even if a matching
// HTMLFrameTreeManager is found. This is useful for tests (or debugging) that
// want to synthesize what happens with multi-processes in a single process.
extern const char kOOPIFAlwaysCreateNewFrameTree[];

}  // namespace switches
}  // namespace web_view

#endif  // COMPONENTS_WEB_VIEW_WEB_VIEW_SWITCHES_H_
