// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PRERENDER_TYPES_H_
#define CHROME_COMMON_PRERENDER_TYPES_H_

namespace prerender {

// PrerenderRelType is a bitfield since multiple rel attributes can be set
// on the same link. Must be the same as blink::WebPrerenderRelType.
enum PrerenderRelType {
  PrerenderRelTypePrerender = 0x1,
  PrerenderRelTypeNext = 0x2,
};

}  // namespace prerender

#endif  // CHROME_COMMON_PRERENDER_TYPES_H_
