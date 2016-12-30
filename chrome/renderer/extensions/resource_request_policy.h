// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_RESOURCE_REQUEST_POLICY_H_
#define CHROME_RENDERER_EXTENSIONS_RESOURCE_REQUEST_POLICY_H_

#include "base/macros.h"
#include "ui/base/page_transition_types.h"

class GURL;

namespace blink {
class WebLocalFrame;
}

namespace extensions {

class Dispatcher;

// Encapsulates the policy for when chrome-extension:// URLs can be requested.
class ResourceRequestPolicy {
 public:
  explicit ResourceRequestPolicy(Dispatcher* dispatcher);

  // Returns true if the chrome-extension:// |resource_url| can be requested
  // from |frame_url|. In some cases this decision is made based upon how
  // this request was generated. Web triggered transitions are more restrictive
  // than those triggered through UI.
  bool CanRequestResource(const GURL& resource_url,
                          blink::WebLocalFrame* frame,
                          ui::PageTransition transition_type);

 private:
  Dispatcher* dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(ResourceRequestPolicy);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_RESOURCE_REQUEST_POLICY_H_
