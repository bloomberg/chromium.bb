// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PRERENDER_PRERENDERING_SUPPORT_H_
#define CHROME_RENDERER_PRERENDER_PRERENDERING_SUPPORT_H_
#pragma once

#include "base/compiler_specific.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebPrerenderingSupport.h"

namespace prerender {

// Implements the interface that provides Prerendering to WebKit.
class PrerenderingSupport : public WebKit::WebPrerenderingSupport {
 public:
  virtual ~PrerenderingSupport();

  virtual void add(const WebKit::WebPrerender& prerender) OVERRIDE;
  virtual void cancel(const WebKit::WebPrerender& prerender) OVERRIDE;
  virtual void abandon(const WebKit::WebPrerender& prerender) OVERRIDE;
};

}  // namespace prerender

#endif  // CHROME_RENDERER_PRERENDER_PRERENDERING_SUPPORT_H_

