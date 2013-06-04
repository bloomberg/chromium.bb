// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRERENDER_CONDITION_NETWORK_H_
#define CHROME_BROWSER_CHROMEOS_PRERENDER_CONDITION_NETWORK_H_

#include "base/compiler_specific.h"
#include "chrome/browser/prerender/prerender_condition.h"

namespace chromeos {

class PrerenderConditionNetwork : public prerender::PrerenderCondition {
 public:
  PrerenderConditionNetwork();
  virtual ~PrerenderConditionNetwork();

  // prerender::PrerenderCondition
  virtual bool CanPrerender() const OVERRIDE;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRERENDER_CONDITION_NETWORK_H_
