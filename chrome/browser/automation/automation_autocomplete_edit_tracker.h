// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_AUTOCOMPLETE_EDIT_TRACKER_H_
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_AUTOCOMPLETE_EDIT_TRACKER_H_
#pragma once

#include "chrome/browser/automation/automation_resource_tracker.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"

class AutomationAutocompleteEditTracker
    : public AutomationResourceTracker<OmniboxView*> {
 public:
  explicit AutomationAutocompleteEditTracker(IPC::Message::Sender* automation);
  virtual ~AutomationAutocompleteEditTracker();
  virtual void AddObserver(OmniboxView* resource);
  virtual void RemoveObserver(OmniboxView* resource);
};

#endif  // CHROME_BROWSER_AUTOMATION_AUTOMATION_AUTOCOMPLETE_EDIT_TRACKER_H_
