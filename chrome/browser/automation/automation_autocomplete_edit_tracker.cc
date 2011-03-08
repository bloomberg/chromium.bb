// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_autocomplete_edit_tracker.h"

#include "content/common/notification_source.h"
#include "content/common/notification_type.h"

AutomationAutocompleteEditTracker::AutomationAutocompleteEditTracker(
    IPC::Message::Sender* automation)
    : AutomationResourceTracker<AutocompleteEditView*>(automation) {
}

AutomationAutocompleteEditTracker::~AutomationAutocompleteEditTracker() {
}

void AutomationAutocompleteEditTracker::AddObserver(
    AutocompleteEditView* resource) {
  registrar_.Add(this, NotificationType::AUTOCOMPLETE_EDIT_DESTROYED,
                 Source<AutocompleteEditView>(resource));
}

void AutomationAutocompleteEditTracker::RemoveObserver(
    AutocompleteEditView* resource) {
  registrar_.Remove(this, NotificationType::AUTOCOMPLETE_EDIT_DESTROYED,
                    Source<AutocompleteEditView>(resource));
}
