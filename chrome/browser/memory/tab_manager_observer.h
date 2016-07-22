// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEMORY_TAB_MANAGER_OBSERVER_H_
#define CHROME_BROWSER_MEMORY_TAB_MANAGER_OBSERVER_H_

namespace content {
class WebContents;
}

namespace memory {

// Interface for objects that wish to be notified of changes in TabManager.
class TabManagerObserver {
 public:
  // Invoked when the Discarded state changes.
  // Sends the WebContents of the tab that had the discarded property changed
  // and the current state to let observers know if it is discarderd or not.
  virtual void OnDiscardedStateChange(content::WebContents* contents,
                                      bool is_discarded);

  // Invoked when the auto-discardable state changes.
  virtual void OnAutoDiscardableStateChange(content::WebContents* contents,
                                            bool is_auto_discardable);

 protected:
  virtual ~TabManagerObserver();
};

}  // namespace memory

#endif  // CHROME_BROWSER_MEMORY_TAB_MANAGER_OBSERVER_H_
