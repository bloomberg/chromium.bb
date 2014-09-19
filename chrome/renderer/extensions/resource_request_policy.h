// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_RESOURCE_REQUEST_POLICY_H_
#define CHROME_RENDERER_EXTENSIONS_RESOURCE_REQUEST_POLICY_H_

#include "ui/base/page_transition_types.h"

class GURL;

namespace blink {
class WebFrame;
}

namespace extensions {

class ExtensionSet;

// Encapsulates the policy for when chrome-extension:// and
// chrome-extension-resource:// URLs can be requested.
class ResourceRequestPolicy {
 public:
  // Returns true if the chrome-extension:// |resource_url| can be requested
  // from |frame_url|. In some cases this decision is made based upon how
  // this request was generated. Web triggered transitions are more restrictive
  // than those triggered through UI.
  static bool CanRequestResource(const GURL& resource_url,
                                 blink::WebFrame* frame,
                                 ui::PageTransition transition_type,
                                 const ExtensionSet* loaded_extensions);
  // Returns true if the chrome-extension-resource:// |resource_url| can be
  // requested from |frame_url|.
  static bool CanRequestExtensionResourceScheme(const GURL& resource_url,
                                                blink::WebFrame* frame);

 private:
  ResourceRequestPolicy();
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_RESOURCE_REQUEST_POLICY_H_
