// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/midi_permission_context.h"

#include "chrome/browser/content_settings/permission_request_id.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "url/gurl.h"

MidiPermissionContext::MidiPermissionContext(Profile* profile)
    : PermissionContextBase(profile, CONTENT_SETTINGS_TYPE_MIDI_SYSEX) {
}

MidiPermissionContext::~MidiPermissionContext() {
}


void MidiPermissionContext::UpdateTabContext(const PermissionRequestID& id,
                                             const GURL& requesting_frame,
                                             bool allowed) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::Get(id.render_process_id(),
                                      id.render_view_id());
  if (content_settings) {
    if (allowed)
      content_settings->OnMidiSysExAccessed(requesting_frame);
    else
      content_settings->OnMidiSysExAccessBlocked(requesting_frame);
  }
}
