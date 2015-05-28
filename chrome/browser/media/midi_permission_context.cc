// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/midi_permission_context.h"

#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "components/content_settings/core/common/permission_request_id.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/common/origin_util.h"
#include "url/gurl.h"

MidiPermissionContext::MidiPermissionContext(Profile* profile)
    : PermissionContextBase(profile, CONTENT_SETTINGS_TYPE_MIDI_SYSEX) {
}

MidiPermissionContext::~MidiPermissionContext() {
}

ContentSetting MidiPermissionContext::GetPermissionStatus(
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  if (requesting_origin.is_valid() &&
      !content::IsOriginSecure(requesting_origin)) {
    return CONTENT_SETTING_BLOCK;
  }

  return PermissionContextBase::GetPermissionStatus(requesting_origin,
                                                    embedding_origin);
}

void MidiPermissionContext::DecidePermission(
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    bool user_gesture,
    const BrowserPermissionCallback& callback) {
  if (requesting_origin.is_valid() &&
      !content::IsOriginSecure(requesting_origin)) {
    NotifyPermissionSet(id, requesting_origin, embedding_origin, callback,
                        false /* persist */, CONTENT_SETTING_BLOCK);
    return;
  }

  PermissionContextBase::DecidePermission(
      web_contents, id, requesting_origin,
      embedding_origin, user_gesture, callback);
}

void MidiPermissionContext::UpdateTabContext(const PermissionRequestID& id,
                                             const GURL& requesting_frame,
                                             bool allowed) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::Get(id.render_process_id(),
                                      id.render_view_id());
  if (!content_settings)
    return;

  if (allowed) {
    content_settings->OnMidiSysExAccessed(requesting_frame);

    content::ChildProcessSecurityPolicy::GetInstance()->
        GrantSendMidiSysExMessage(id.render_process_id());
  } else {
    content_settings->OnMidiSysExAccessBlocked(requesting_frame);
  }
}
