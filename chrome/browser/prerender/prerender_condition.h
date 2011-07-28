// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_CONDITION_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_CONDITION_H_

namespace prerender {

// PrerenderCondition's dictate whether prerendering can happen.
// All conditions must be met before a prerender is allowed.
class PrerenderCondition {
 public:
  virtual ~PrerenderCondition() {}
  virtual bool CanPrerender() const = 0;
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_CONDITION_H_
