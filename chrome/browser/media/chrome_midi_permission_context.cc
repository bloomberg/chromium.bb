// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/chrome_midi_permission_context.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/permission_queue_controller.h"
#include "chrome/browser/content_settings/permission_request_id.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

ChromeMIDIPermissionContext::ChromeMIDIPermissionContext(Profile* profile)
    : profile_(profile),
      shutting_down_(false) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

ChromeMIDIPermissionContext::~ChromeMIDIPermissionContext() {
  DCHECK(!permission_queue_controller_);
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

void ChromeMIDIPermissionContext::Shutdown() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  permission_queue_controller_.reset();
  shutting_down_ = true;
}

void ChromeMIDIPermissionContext::RequestMIDISysExPermission(
    int render_process_id,
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame,
    const content::BrowserContext::MIDISysExPermissionCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!shutting_down_);

  // TODO(toyoshim): Support Extension's manifest declared permission.
  // http://crbug.com/266338 .

  content::WebContents* web_contents =
    tab_util::GetWebContentsByID(render_process_id, render_view_id);

  // The page doesn't exist any more.
  if (!web_contents)
    return;

  const PermissionRequestID id(render_process_id, render_view_id, bridge_id);

  GURL embedder = web_contents->GetURL();
  if (!requesting_frame.is_valid() || !embedder.is_valid()) {
    LOG(WARNING) << "Attempt to use MIDI sysex from an invalid URL: "
                 << requesting_frame << "," << embedder
                 << " (Web MIDI is not supported in popups)";
    PermissionDecided(id, requesting_frame, embedder, callback, false);
    return;
  }

  DecidePermission(id, requesting_frame, embedder, callback);
}

void ChromeMIDIPermissionContext::CancelMIDISysExPermissionRequest(
    int render_process_id,
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame) {
  CancelPendingInfoBarRequest(
      PermissionRequestID(render_process_id, render_view_id, bridge_id));
}

void ChromeMIDIPermissionContext::DecidePermission(
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    const GURL& embedder,
    const content::BrowserContext::MIDISysExPermissionCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  ContentSetting content_setting =
      profile_->GetHostContentSettingsMap()->GetContentSetting(
          requesting_frame,
          embedder,
          CONTENT_SETTINGS_TYPE_MIDI_SYSEX,
          std::string());
  switch (content_setting) {
    case CONTENT_SETTING_BLOCK:
      PermissionDecided(id, requesting_frame, embedder, callback, false);
      break;
    case CONTENT_SETTING_ALLOW:
      PermissionDecided(id, requesting_frame, embedder, callback, true);
      break;
    default:
      GetQueueController()->CreateInfoBarRequest(
          id, requesting_frame, embedder, base::Bind(
              &ChromeMIDIPermissionContext::NotifyPermissionSet,
              base::Unretained(this), id, requesting_frame, callback));
  }
}

void ChromeMIDIPermissionContext::PermissionDecided(
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    const GURL& embedder,
    const content::BrowserContext::MIDISysExPermissionCallback& callback,
    bool allowed) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  NotifyPermissionSet(id, requesting_frame, callback, allowed);
}

void ChromeMIDIPermissionContext::NotifyPermissionSet(
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    const content::BrowserContext::MIDISysExPermissionCallback& callback,
    bool allowed) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::Get(id.render_process_id(),
                                      id.render_view_id());
  if (content_settings) {
    if (allowed)
      content_settings->OnMIDISysExAccessed(requesting_frame);
    else
      content_settings->OnMIDISysExAccessBlocked(requesting_frame);
  }

  callback.Run(allowed);
}

PermissionQueueController* ChromeMIDIPermissionContext::GetQueueController() {
  if (!permission_queue_controller_) {
    permission_queue_controller_.reset(
        new PermissionQueueController(profile_,
                                      CONTENT_SETTINGS_TYPE_MIDI_SYSEX));
  }
  return permission_queue_controller_.get();
}

void ChromeMIDIPermissionContext::CancelPendingInfoBarRequest(
    const PermissionRequestID& id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (shutting_down_)
    return;
  GetQueueController()->CancelInfoBarRequest(id);
}
