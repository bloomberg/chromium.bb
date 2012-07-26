// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_METRO_PINNED_STATE_OBSERVER_H_
#define CHROME_BROWSER_UI_METRO_PINNED_STATE_OBSERVER_H_

namespace content {
class WebContents;
}

// Objects implement this interface to get notified about changes in the
// metro pinned state.
class MetroPinnedStateObserver {
 public:
  // Notification that the pinned state of the current URL changed.
  virtual void MetroPinnedStateChanged(content::WebContents* source,
                                       bool is_pinned) = 0;

 protected:
  virtual ~MetroPinnedStateObserver() {}
};

#endif  // CHROME_BROWSER_UI_METRO_PINNED_STATE_OBSERVER_H_
