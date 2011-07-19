// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_MOCK_TAB_EVENT_OBSERVER_H_
#define CHROME_BROWSER_AUTOMATION_MOCK_TAB_EVENT_OBSERVER_H_
#pragma once

#include "chrome/browser/automation/automation_tab_helper.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockTabEventObserver : public TabEventObserver {
 public:
  MockTabEventObserver();
  // Convenience constructor for observing the |tab_helper| on creation.
  explicit MockTabEventObserver(AutomationTabHelper* tab_helper);
  virtual ~MockTabEventObserver();

  // Promote these to public for testing purposes.
  void StartObserving(AutomationTabHelper* tab_helper);
  void StopObserving(AutomationTabHelper* tab_helper);

  MOCK_METHOD1(OnFirstPendingLoad, void(TabContents* tab_contents));
  MOCK_METHOD1(OnNoMorePendingLoads, void(TabContents* tab_contents));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTabEventObserver);
};

#endif  // CHROME_BROWSER_AUTOMATION_MOCK_TAB_EVENT_OBSERVER_H_
