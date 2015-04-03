// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CONTENT_DISTILLABLE_PAGE_UTILS_H_
#define COMPONENTS_DOM_DISTILLER_CONTENT_DISTILLABLE_PAGE_UTILS_H_

#include "base/callback.h"
#include "content/public/browser/web_contents.h"

namespace dom_distiller {

class DistillablePageDetector;

void IsOpenGraphArticle(content::WebContents* web_contents,
                        base::Callback<void(bool)> callback);
void IsDistillablePage(content::WebContents* web_contents,
                       base::Callback<void(bool)> callback);
// The passed detector must be alive until after the callback is called.
void IsDistillablePageForDetector(content::WebContents* web_contents,
                                  const DistillablePageDetector* detector,
                                  base::Callback<void(bool)> callback);
}

#endif  // COMPONENTS_DOM_DISTILLER_CONTENT_DISTILLABLE_PAGE_UTILS_H_
