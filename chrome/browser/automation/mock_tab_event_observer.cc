// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/mock_tab_event_observer.h"

MockTabEventObserver::MockTabEventObserver() { }

MockTabEventObserver::MockTabEventObserver(AutomationTabHelper* tab_helper) {
  TabEventObserver::StartObserving(tab_helper);
}

MockTabEventObserver::~MockTabEventObserver() { }

void MockTabEventObserver::StartObserving(AutomationTabHelper* tab_helper) {
  TabEventObserver::StartObserving(tab_helper);
}

void MockTabEventObserver::StopObserving(AutomationTabHelper* tab_helper) {
  TabEventObserver::StopObserving(tab_helper);
}
