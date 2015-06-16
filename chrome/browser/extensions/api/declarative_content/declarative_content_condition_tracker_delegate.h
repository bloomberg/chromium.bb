// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_CONDITION_TRACKER_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_CONDITION_TRACKER_DELEGATE_H_

#include "base/macros.h"

namespace content {
class BrowserContext;
class WebContents;
}

namespace extensions {

// Interface that allows the declarative content condition trackers to request
// condition evaluation.
class DeclarativeContentConditionTrackerDelegate {
 public:
  // Requests re-evaluation of conditions for |contents|. This must be called
  // whenever the state monitored by the condition changes, even if the value of
  // the condition itself doesn't change.
  virtual void RequestEvaluation(content::WebContents* contents) = 0;

  // Returns true if the evaluator should manage condition state for |context|.
  virtual bool ShouldManageConditionsForBrowserContext(
      content::BrowserContext* context) = 0;

 protected:
  DeclarativeContentConditionTrackerDelegate();
  virtual ~DeclarativeContentConditionTrackerDelegate();

 private:
  DISALLOW_COPY_AND_ASSIGN(DeclarativeContentConditionTrackerDelegate);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_DECLARATIVE_CONTENT_CONDITION_TRACKER_DELEGATE_H_
