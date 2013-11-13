// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PRERENDER_CONDITION_PLATFORM_H_
#define CHROME_BROWSER_ANDROID_PRERENDER_CONDITION_PLATFORM_H_

#include "base/compiler_specific.h"
#include "base/supports_user_data.h"
#include "chrome/browser/prerender/prerender_condition.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace android {

class PrerenderConditionPlatform : public prerender::PrerenderCondition {
 public:
  explicit PrerenderConditionPlatform(content::BrowserContext* context);
  virtual ~PrerenderConditionPlatform();

  // prerender::PrerenderCondition
  virtual bool CanPrerender() const OVERRIDE;

  static void SetEnabled(content::BrowserContext* context, bool enabled);
 private:
  content::BrowserContext* context_;
  DISALLOW_COPY_AND_ASSIGN(PrerenderConditionPlatform);
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_PRERENDER_CONDITION_PLATFORM_H_
