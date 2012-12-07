// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tab_capture/tab_capture_registry.h"

#include "content/public/browser/browser_thread.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/media/media_internals.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"

namespace events = extensions::event_names;
using content::BrowserThread;

namespace extensions {

TabCaptureRegistry::TabCaptureRegistry(Profile* profile)
    : proxy_(new MediaObserverProxy()), profile_(profile) {
  proxy_->Attach(this);
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile_));
}

TabCaptureRegistry::~TabCaptureRegistry() {
  proxy_->Detach();
}

void TabCaptureRegistry::HandleRequestUpdateOnUIThread(
    const content::MediaStreamDevice& device,
    const content::MediaRequestState new_state) {
  EventRouter* router = profile_ ?
      extensions::ExtensionSystem::Get(profile_)->event_router() : NULL;
  if (!router)
    return;

  if (requests_.find(device.device_id) == requests_.end())
    return;

  tab_capture::TabCaptureState state =
      tab_capture::TAB_CAPTURE_TAB_CAPTURE_STATE_NONE;
  switch (new_state) {
    case content::MEDIA_REQUEST_STATE_REQUESTED:
      state = tab_capture::TAB_CAPTURE_TAB_CAPTURE_STATE_REQUESTED;
      break;
    case content::MEDIA_REQUEST_STATE_PENDING_APPROVAL:
      state = tab_capture::TAB_CAPTURE_TAB_CAPTURE_STATE_PENDING;
      break;
    case content::MEDIA_REQUEST_STATE_DONE:
      state = tab_capture::TAB_CAPTURE_TAB_CAPTURE_STATE_ACTIVE;
      break;
    case content::MEDIA_REQUEST_STATE_CLOSING:
      state = tab_capture::TAB_CAPTURE_TAB_CAPTURE_STATE_STOPPED;
      break;
    case content::MEDIA_REQUEST_STATE_ERROR:
      state = tab_capture::TAB_CAPTURE_TAB_CAPTURE_STATE_ERROR;
      break;
    default:
      // TODO(justinlin): Implement muted state notification.
      break;
  }

  if (state == tab_capture::TAB_CAPTURE_TAB_CAPTURE_STATE_NONE) {
    // This is a state we don't handle.
    return;
  }

  TabCaptureRegistry::TabCaptureRequest& request_info =
      requests_[device.device_id];
  request_info.status = state;

  scoped_ptr<tab_capture::CaptureInfo> info(new tab_capture::CaptureInfo());
  info->tab_id = request_info.tab_id;
  info->status = request_info.status;

  scoped_ptr<base::ListValue> args(new ListValue());
  args->Append(info->ToValue().release());
  scoped_ptr<Event> event(new Event(
      events::kOnTabCaptureStatusChanged, args.Pass()));
  event->restrict_to_profile = profile_;
  router->DispatchEventToExtension(request_info.extension_id, event.Pass());
}

const TabCaptureRegistry::CaptureRequestList
    TabCaptureRegistry::GetCapturedTabs(const std::string& extension_id) {
  CaptureRequestList list;
  for (DeviceCaptureRequestMap::iterator it = requests_.begin();
       it != requests_.end(); ++it) {
    if (it->second.extension_id == extension_id) {
      list.push_back(it->second);
    }
  }
  return list;
}

void TabCaptureRegistry::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      // Cleanup all the requested media streams for this extension. We might
      // accumulate too many requests left in the closed state otherwise.
      std::string extension_id =
          content::Details<extensions::UnloadedExtensionInfo>(details)->
              extension->id();
      for (DeviceCaptureRequestMap::iterator it = requests_.begin();
           it != requests_.end();) {
        if (it->second.extension_id == extension_id) {
          requests_.erase(it++);
        } else {
          ++it;
        }
      }
      break;
    }
  }
}

bool TabCaptureRegistry::AddRequest(
    const std::string& key, const TabCaptureRequest& request) {
  // Currently, we do not allow multiple active captures for same tab.
  DCHECK(!key.empty());
  if (requests_.find(key) != requests_.end())
    if (requests_[key].status !=
        tab_capture::TAB_CAPTURE_TAB_CAPTURE_STATE_STOPPED &&
        requests_[key].status !=
        tab_capture::TAB_CAPTURE_TAB_CAPTURE_STATE_ERROR)
      return false;
  requests_[key] = request;
  return true;
}

bool TabCaptureRegistry::VerifyRequest(const std::string& key) {
  return requests_.find(key) != requests_.end();
}

void TabCaptureRegistry::MediaObserverProxy::Attach(
    TabCaptureRegistry* request_handler) {
  handler_ = request_handler;
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&TabCaptureRegistry::MediaObserverProxy::
                 RegisterAsMediaObserverOnIOThread, this, false));
}

void TabCaptureRegistry::MediaObserverProxy::Detach() {
  handler_ = NULL;
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      base::Bind(&TabCaptureRegistry::MediaObserverProxy::
                 RegisterAsMediaObserverOnIOThread, this, true));
}

void TabCaptureRegistry::MediaObserverProxy::RegisterAsMediaObserverOnIOThread(
      bool unregister) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (MediaInternals::GetInstance()) {
    if (!unregister)
      MediaInternals::GetInstance()->AddObserver(this);
    else
      MediaInternals::GetInstance()->RemoveObserver(this);
  }
}

void TabCaptureRegistry::MediaObserverProxy::OnRequestUpdate(
    const content::MediaStreamDevice& device,
    const content::MediaRequestState new_state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // TODO(justinlin): We drop audio device events since they will occur in
  // parallel with the video device events (we would get duplicate events). When
  // audio mirroring is implemented, we will want to grab those events when
  // video is not requested.
  if (device.type != content::MEDIA_TAB_VIDEO_CAPTURE)
    return;

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      base::Bind(&TabCaptureRegistry::MediaObserverProxy::UpdateOnUIThread,
          this, device, new_state));
}

void TabCaptureRegistry::MediaObserverProxy::UpdateOnUIThread(
    const content::MediaStreamDevice& device,
    const content::MediaRequestState new_state) {
  if (handler_)
    handler_->HandleRequestUpdateOnUIThread(device, new_state);
}

}  // namespace extensions
