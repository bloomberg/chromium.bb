// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_LOCAL_CARD_MIGRATION_CONTROLLER_OBSERVER_H_
#define CHROME_BROWSER_UI_AUTOFILL_LOCAL_CARD_MIGRATION_CONTROLLER_OBSERVER_H_

class LocalCardMigrationControllerObserver {
 public:
  virtual void OnMigrationNoLongerAvailable() = 0;

 protected:
  virtual ~LocalCardMigrationControllerObserver() = default;
};

#endif  // CHROME_BROWSER_UI_AUTOFILL_LOCAL_CARD_MIGRATION_CONTROLLER_OBSERVER_H_
