// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/omnibox_watcher.h"

#include "base/time/time.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/omnibox/omnibox_log.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "components/omnibox/autocomplete_result.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"

namespace safe_browsing {

OmniboxWatcher::OmniboxWatcher(Profile* profile,
                               const AddIncidentCallback& callback):
    incident_callback_(callback) {
  registrar_.Add(this, chrome::NOTIFICATION_OMNIBOX_OPENED_URL,
                 content::Source<Profile>(profile));
}

OmniboxWatcher::~OmniboxWatcher() {
}

void OmniboxWatcher::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_OMNIBOX_OPENED_URL, type);
  const OmniboxLog* log = content::Details<OmniboxLog>(details).ptr();
  const AutocompleteMatch& selected_suggestion =
      log->result.match_at(log->selected_index);
  // Users tend not to type very long strings explicitly (especially without
  // using the paste-and-go option), and certainly not in under a second.
  // No normal person can type URLs that fast!  Navigating to a URL as a
  // result of such typing is suspicious.
  // TODO(mpearson): Add support for suspicious queries.
  if (!log->is_paste_and_go && !log->last_action_was_paste &&
      log->is_popup_open && (log->text.length() > 200) &&
      (log->elapsed_time_since_user_first_modified_omnibox <
       base::TimeDelta::FromSeconds(1)) &&
      !AutocompleteMatch::IsSearchType(selected_suggestion.type)) {
    scoped_ptr<ClientIncidentReport_IncidentData> incident_data(
        new ClientIncidentReport_IncidentData());
    const GURL& origin = selected_suggestion.destination_url.GetOrigin();
    incident_data->mutable_omnibox_interaction()->set_origin(
        origin.possibly_invalid_spec());
    incident_callback_.Run(incident_data.Pass());
  }
}

}  // namespace safe_browsing
