// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_CHROME_APP_LIST_MODEL_UPDATER_DELEGATE_H_
#define CHROME_BROWSER_UI_APP_LIST_CHROME_APP_LIST_MODEL_UPDATER_DELEGATE_H_

class ChromeAppListItem;

// A delegate interface of ChromeAppListModelUpdater to perform
// additional work on ChromeAppListItem changes.
class ChromeAppListModelUpdaterDelegate {
 public:
  // Triggered after an item has been added to the model.
  virtual void OnAppListItemAdded(ChromeAppListItem* item) {}

  // Triggered just before an item is deleted from the model.
  virtual void OnAppListItemWillBeDeleted(ChromeAppListItem* item) {}

  // Triggered after an item has moved, changed folders, or changed properties.
  virtual void OnAppListItemUpdated(ChromeAppListItem* item) {}

 protected:
  virtual ~ChromeAppListModelUpdaterDelegate() {}
};

#endif  // CHROME_BROWSER_UI_APP_LIST_CHROME_APP_LIST_MODEL_UPDATER_DELEGATE_H_
