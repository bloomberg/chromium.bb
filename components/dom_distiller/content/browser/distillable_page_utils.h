// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLABLE_PAGE_UTILS_H_
#define COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLABLE_PAGE_UTILS_H_

#include "base/callback.h"

namespace content {
class WebContents;
}  // namespace content

namespace dom_distiller {

class DistillablePageDetector;

// Uses the provided DistillablePageDetector to detect if the page is
// distillable. The passed detector must be alive until after the callback is
// called.
//
// |web_contents| and |detector| must be non-null.
void IsDistillablePageForDetector(content::WebContents* web_contents,
                                  const DistillablePageDetector* detector,
                                  base::Callback<void(bool)> callback);

// Set the delegate to receive the result of whether the page is distillable.
//
// |web_contents| must be non-null.
using DistillabilityDelegate =
    base::RepeatingCallback<void(bool /* is_distillable */,
                                 bool /* is_last */,
                                 bool /* is_mobile_friendly */)>;
void SetDelegate(content::WebContents* web_contents,
                 DistillabilityDelegate delegate);

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CONTENT_BROWSER_DISTILLABLE_PAGE_UTILS_H_
