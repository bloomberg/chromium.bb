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
#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"
#include "chrome/browser/ui/website_settings/permission_bubble_request.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"

class MidiPermissionRequest : public PermissionBubbleRequest {
 public:
  MidiPermissionRequest(
      ChromeMidiPermissionContext* context,
      const PermissionRequestID& id,
      const GURL& requesting_frame,
      bool user_gesture,
      const std::string& display_languages,
      const content::BrowserContext::MidiSysExPermissionCallback& callback);
  virtual ~MidiPermissionRequest();

  // PermissionBubbleDelegate:
  virtual int GetIconID() const OVERRIDE;
  virtual base::string16 GetMessageText() const OVERRIDE;
  virtual base::string16 GetMessageTextFragment() const OVERRIDE;
  virtual bool HasUserGesture() const OVERRIDE;
  virtual GURL GetRequestingHostname() const OVERRIDE;
  virtual void PermissionGranted() OVERRIDE;
  virtual void PermissionDenied() OVERRIDE;
  virtual void Cancelled() OVERRIDE;
  virtual void RequestFinished() OVERRIDE;

 private:
  ChromeMidiPermissionContext* context_;
  const PermissionRequestID id_;
  GURL requesting_frame_;
  bool user_gesture_;
  std::string display_languages_;
  const content::BrowserContext::MidiSysExPermissionCallback& callback_;
  bool is_finished_;

  DISALLOW_COPY_AND_ASSIGN(MidiPermissionRequest);
};

MidiPermissionRequest::MidiPermissionRequest(
    ChromeMidiPermissionContext* context,
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    bool user_gesture,
    const std::string& display_languages,
    const content::BrowserContext::MidiSysExPermissionCallback& callback)
    : context_(context),
      id_(id),
      requesting_frame_(requesting_frame),
      user_gesture_(user_gesture),
      display_languages_(display_languages),
      callback_(callback),
      is_finished_(false) {}

MidiPermissionRequest::~MidiPermissionRequest() {
  DCHECK(is_finished_);
}

int MidiPermissionRequest::GetIconID() const {
  return IDR_ALLOWED_MIDI_SYSEX;
}

base::string16 MidiPermissionRequest::GetMessageText() const {
  return l10n_util::GetStringFUTF16(
      IDS_MIDI_SYSEX_INFOBAR_QUESTION,
      net::FormatUrl(requesting_frame_.GetOrigin(), display_languages_));
}

base::string16 MidiPermissionRequest::GetMessageTextFragment() const {
  return l10n_util::GetStringUTF16(IDS_MIDI_SYSEX_PERMISSION_FRAGMENT);
}

bool MidiPermissionRequest::HasUserGesture() const {
  return user_gesture_;
}

GURL MidiPermissionRequest::GetRequestingHostname() const {
  return requesting_frame_;
}

void MidiPermissionRequest::PermissionGranted() {
  context_->NotifyPermissionSet(id_, requesting_frame_, callback_, true);
}

void MidiPermissionRequest::PermissionDenied() {
  context_->NotifyPermissionSet(id_, requesting_frame_, callback_, false);
}

void MidiPermissionRequest::Cancelled() {
}

void MidiPermissionRequest::RequestFinished() {
  is_finished_ = true;
  // Deletes 'this'.
  context_->RequestFinished(this);
}

ChromeMidiPermissionContext::ChromeMidiPermissionContext(Profile* profile)
    : profile_(profile),
      shutting_down_(false) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

ChromeMidiPermissionContext::~ChromeMidiPermissionContext() {
  DCHECK(!permission_queue_controller_);
  DCHECK(pending_requests_.empty());
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

void ChromeMidiPermissionContext::Shutdown() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  permission_queue_controller_.reset();
  shutting_down_ = true;
}

void ChromeMidiPermissionContext::RequestMidiSysExPermission(
    int render_process_id,
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame,
    bool user_gesture,
    const content::BrowserContext::MidiSysExPermissionCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!shutting_down_);

  // TODO(toyoshim): Support Extension's manifest declared permission.
  // See http://crbug.com/266338.

  content::WebContents* web_contents =
    tab_util::GetWebContentsByID(render_process_id, render_view_id);

  // The page doesn't exist any more.
  if (!web_contents)
    return;

  const PermissionRequestID id(
      render_process_id, render_view_id, bridge_id, GURL());

  GURL embedder = web_contents->GetURL();
  // |requesting_frame| can be empty and invalid when the frame is a local
  // file. Here local files should be granted to show an infobar.
  // Any user's action will not be stored to content settings data base.
  if ((!requesting_frame.is_valid() && !requesting_frame.is_empty()) ||
      !embedder.is_valid()) {
    LOG(WARNING) << "Attempt to use MIDI sysex from an invalid URL: "
                 << requesting_frame << "," << embedder
                 << " (Web MIDI is not supported in popups)";
    PermissionDecided(id, requesting_frame, embedder, callback, false);
    return;
  }

  DecidePermission(web_contents, id, requesting_frame, embedder, user_gesture,
                   callback);
}

void ChromeMidiPermissionContext::CancelMidiSysExPermissionRequest(
    int render_process_id,
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame) {
  CancelPendingInfobarRequest(
      PermissionRequestID(
          render_process_id, render_view_id, bridge_id, GURL()));
}

void ChromeMidiPermissionContext::DecidePermission(
    content::WebContents* web_contents,
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    const GURL& embedder,
    bool user_gesture,
    const content::BrowserContext::MidiSysExPermissionCallback& callback) {
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
      if (PermissionBubbleManager::Enabled()) {
        PermissionBubbleManager* bubble_manager =
            PermissionBubbleManager::FromWebContents(web_contents);
        if (bubble_manager) {
          scoped_ptr<MidiPermissionRequest> request_ptr(
              new MidiPermissionRequest(
                  this, id, requesting_frame, user_gesture,
                  profile_->GetPrefs()->GetString(prefs::kAcceptLanguages),
                  callback));
          MidiPermissionRequest* request = request_ptr.get();
          bool inserted = pending_requests_.add(
              id.ToString(), request_ptr.Pass()).second;
          DCHECK(inserted) << "Duplicate id " << id.ToString();
          bubble_manager->AddRequest(request);
        }
        return;
      }

      // TODO(gbillock): Delete this and the infobar delegate when
      // we're using only bubbles. crbug.com/337458
      GetQueueController()->CreateInfoBarRequest(
          id, requesting_frame, embedder, std::string(), base::Bind(
              &ChromeMidiPermissionContext::NotifyPermissionSet,
              base::Unretained(this), id, requesting_frame, callback));
  }
}

void ChromeMidiPermissionContext::PermissionDecided(
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    const GURL& embedder,
    const content::BrowserContext::MidiSysExPermissionCallback& callback,
    bool allowed) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  NotifyPermissionSet(id, requesting_frame, callback, allowed);
}

void ChromeMidiPermissionContext::NotifyPermissionSet(
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    const content::BrowserContext::MidiSysExPermissionCallback& callback,
    bool allowed) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::Get(id.render_process_id(),
                                      id.render_view_id());
  if (content_settings) {
    if (allowed)
      content_settings->OnMidiSysExAccessed(requesting_frame);
    else
      content_settings->OnMidiSysExAccessBlocked(requesting_frame);
  }

  callback.Run(allowed);
}

PermissionQueueController* ChromeMidiPermissionContext::GetQueueController() {
  if (!permission_queue_controller_) {
    permission_queue_controller_.reset(
        new PermissionQueueController(profile_,
                                      CONTENT_SETTINGS_TYPE_MIDI_SYSEX));
  }
  return permission_queue_controller_.get();
}

void ChromeMidiPermissionContext::RequestFinished(
    MidiPermissionRequest* request) {
  base::ScopedPtrHashMap<std::string, MidiPermissionRequest>::iterator it;
  for (it = pending_requests_.begin(); it != pending_requests_.end(); it++) {
    if (it->second == request) {
      pending_requests_.take_and_erase(it);
      return;
    }
  }

  NOTREACHED() << "Missing request";
}

void ChromeMidiPermissionContext::CancelPendingInfobarRequest(
    const PermissionRequestID& id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (shutting_down_)
    return;

  if (PermissionBubbleManager::Enabled()) {
    MidiPermissionRequest* cancelling = pending_requests_.get(id.ToString());
    content::WebContents* web_contents = tab_util::GetWebContentsByID(
        id.render_process_id(), id.render_view_id());
    if (cancelling != NULL && web_contents != NULL &&
        PermissionBubbleManager::FromWebContents(web_contents) != NULL) {
      PermissionBubbleManager::FromWebContents(web_contents)->
          CancelRequest(cancelling);
    }
    return;
  }

  GetQueueController()->CancelInfoBarRequest(id);
}
