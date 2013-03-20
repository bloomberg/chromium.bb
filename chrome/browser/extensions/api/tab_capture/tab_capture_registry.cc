// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tab_capture/tab_capture_registry.h"

#include <utility>

#include "content/public/browser/browser_thread.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

using content::BrowserThread;
using extensions::TabCaptureRegistry;
using extensions::tab_capture::TabCaptureState;

namespace extensions {

class FullscreenObserver : public content::WebContentsObserver {
 public:
  FullscreenObserver(TabCaptureRequest* request,
                     const TabCaptureRegistry* registry);
  virtual ~FullscreenObserver() {}

 private:
  // content::WebContentsObserver implementation.
  virtual void DidShowFullscreenWidget(int routing_id) OVERRIDE;
  virtual void DidDestroyFullscreenWidget(int routing_id) OVERRIDE;

  TabCaptureRequest* request_;
  const TabCaptureRegistry* registry_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenObserver);
};

// Holds all the state related to a tab capture stream.
struct TabCaptureRequest {
  TabCaptureRequest(int render_process_id,
                    int render_view_id,
                    const std::string& extension_id,
                    int tab_id,
                    TabCaptureState status);
  ~TabCaptureRequest();

  const int render_process_id;
  const int render_view_id;
  const std::string extension_id;
  const int tab_id;
  TabCaptureState status;
  TabCaptureState last_status;
  bool fullscreen;
  scoped_ptr<FullscreenObserver> fullscreen_observer;
};

FullscreenObserver::FullscreenObserver(
    TabCaptureRequest* request,
    const TabCaptureRegistry* registry)
    : request_(request),
      registry_(registry) {
  content::RenderViewHost* const rvh =
      content::RenderViewHost::FromID(request->render_process_id,
                                      request->render_view_id);
  Observe(rvh ? content::WebContents::FromRenderViewHost(rvh) : NULL);
}

void FullscreenObserver::DidShowFullscreenWidget(
    int routing_id) {
  request_->fullscreen = true;
  registry_->DispatchStatusChangeEvent(request_);
}

void FullscreenObserver::DidDestroyFullscreenWidget(
    int routing_id) {
  request_->fullscreen = false;
  registry_->DispatchStatusChangeEvent(request_);
}

TabCaptureRequest::TabCaptureRequest(
    int render_process_id,
    int render_view_id,
    const std::string& extension_id,
    const int tab_id,
    TabCaptureState status)
    : render_process_id(render_process_id),
      render_view_id(render_view_id),
      extension_id(extension_id),
      tab_id(tab_id),
      status(status),
      last_status(status),
      fullscreen(false) {
}

TabCaptureRequest::~TabCaptureRequest() {
}

TabCaptureRegistry::TabCaptureRegistry(Profile* profile)
    : profile_(profile) {
  MediaCaptureDevicesDispatcher::GetInstance()->AddObserver(this);
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile_));
  // TODO(justinlin): Hook up HTML5 fullscreen.
}

TabCaptureRegistry::~TabCaptureRegistry() {
  MediaCaptureDevicesDispatcher::GetInstance()->RemoveObserver(this);
}

const TabCaptureRegistry::RegistryCaptureInfo
    TabCaptureRegistry::GetCapturedTabs(const std::string& extension_id) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RegistryCaptureInfo list;
  for (ScopedVector<TabCaptureRequest>::const_iterator it = requests_.begin();
       it != requests_.end(); ++it) {
    if ((*it)->extension_id == extension_id) {
      list.push_back(std::make_pair((*it)->tab_id, (*it)->status));
    }
  }
  return list;
}

void TabCaptureRegistry::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      // Cleanup all the requested media streams for this extension.
      const std::string& extension_id =
          content::Details<extensions::UnloadedExtensionInfo>(details)->
              extension->id();
      for (ScopedVector<TabCaptureRequest>::iterator it = requests_.begin();
           it != requests_.end();) {
        if ((*it)->extension_id == extension_id) {
          it = requests_.erase(it);
        } else {
          ++it;
        }
      }
      break;
    }
    case chrome::NOTIFICATION_FULLSCREEN_CHANGED: {
      // TODO(justinlin): Hook up HTML5 fullscreen.
      break;
    }
  }
}

bool TabCaptureRegistry::AddRequest(int render_process_id,
                                    int render_view_id,
                                    const std::string& extension_id,
                                    int tab_id,
                                    TabCaptureState status) {
  TabCaptureRequest* request = FindCaptureRequest(render_process_id,
                                                  render_view_id);
  // Currently, we do not allow multiple active captures for same tab.
  if (request != NULL) {
    if (request->status != tab_capture::TAB_CAPTURE_STATE_STOPPED &&
        request->status != tab_capture::TAB_CAPTURE_STATE_ERROR) {
      return false;
    } else {
      DeleteCaptureRequest(render_process_id, render_view_id);
    }
  }

  requests_.push_back(new TabCaptureRequest(render_process_id,
                                            render_view_id,
                                            extension_id,
                                            tab_id,
                                            status));
  return true;
}

bool TabCaptureRegistry::VerifyRequest(int render_process_id,
                                       int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DVLOG(1) << "Verifying tabCapture request for "
           << render_process_id << ":" << render_view_id;
  // TODO(justinlin): Verify extension too.
  return (FindCaptureRequest(render_process_id, render_view_id) != NULL);
}

void TabCaptureRegistry::OnRequestUpdate(
    int render_process_id,
    int render_view_id,
    const content::MediaStreamDevice& device,
    const content::MediaRequestState new_state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (device.type != content::MEDIA_TAB_VIDEO_CAPTURE &&
      device.type != content::MEDIA_TAB_AUDIO_CAPTURE) {
    return;
  }

  TabCaptureRequest* request = FindCaptureRequest(render_process_id,
                                                  render_view_id);
  if (request == NULL) {
    // TODO(justinlin): This can happen because the extension's renderer does
    // not seem to always cleanup streams correctly.
    LOG(ERROR) << "Receiving updates for deleted capture request.";
    return;
  }

  bool opening_stream = false;
  bool stopping_stream = false;

  TabCaptureState next_state = tab_capture::TAB_CAPTURE_STATE_NONE;
  switch (new_state) {
    case content::MEDIA_REQUEST_STATE_PENDING_APPROVAL:
      next_state = tab_capture::TAB_CAPTURE_STATE_PENDING;
      break;
    case content::MEDIA_REQUEST_STATE_DONE:
      opening_stream = true;
      next_state = tab_capture::TAB_CAPTURE_STATE_ACTIVE;
      break;
    case content::MEDIA_REQUEST_STATE_CLOSING:
      stopping_stream = true;
      next_state = tab_capture::TAB_CAPTURE_STATE_STOPPED;
      break;
    case content::MEDIA_REQUEST_STATE_ERROR:
      stopping_stream = true;
      next_state = tab_capture::TAB_CAPTURE_STATE_ERROR;
      break;
    case content::MEDIA_REQUEST_STATE_OPENING:
      return;
    case content::MEDIA_REQUEST_STATE_REQUESTED:
    case content::MEDIA_REQUEST_STATE_NOT_REQUESTED:
      NOTREACHED();
      return;
  }

  if (next_state == tab_capture::TAB_CAPTURE_STATE_PENDING &&
      request->status != tab_capture::TAB_CAPTURE_STATE_PENDING &&
      request->status != tab_capture::TAB_CAPTURE_STATE_NONE &&
      request->status != tab_capture::TAB_CAPTURE_STATE_STOPPED &&
      request->status != tab_capture::TAB_CAPTURE_STATE_ERROR) {
    // If we end up trying to grab a new stream while the previous one was never
    // terminated, then something fishy is going on.
    NOTREACHED() << "Trying to capture tab with existing stream.";
    return;
  }

  if (opening_stream) {
    request->fullscreen_observer.reset(new FullscreenObserver(request, this));
  }

  if (stopping_stream) {
    request->fullscreen_observer.reset();
  }

  request->last_status = request->status;
  request->status = next_state;

  // We will get duplicate events if we requested both audio and video, so only
  // send new events.
  if (request->last_status != request->status) {
    DispatchStatusChangeEvent(request);
  }
}

void TabCaptureRegistry::DispatchStatusChangeEvent(
    const TabCaptureRequest* request) const {
  EventRouter* router = profile_ ?
      extensions::ExtensionSystem::Get(profile_)->event_router() : NULL;
  if (!router)
    return;

  scoped_ptr<tab_capture::CaptureInfo> info(new tab_capture::CaptureInfo());
  info->tab_id = request->tab_id;
  info->status = request->status;
  info->fullscreen = request->fullscreen;

  scoped_ptr<base::ListValue> args(new ListValue());
  args->Append(info->ToValue().release());
  scoped_ptr<Event> event(new Event(
      extensions::event_names::kOnTabCaptureStatusChanged, args.Pass()));
  event->restrict_to_profile = profile_;

  router->DispatchEventToExtension(request->extension_id, event.Pass());
}

TabCaptureRequest* TabCaptureRegistry::FindCaptureRequest(
    int render_process_id, int render_view_id) const {
  for (ScopedVector<TabCaptureRequest>::const_iterator it = requests_.begin();
       it != requests_.end(); ++it) {
    if ((*it)->render_process_id == render_process_id &&
        (*it)->render_view_id == render_view_id) {
      return *it;
    }
  }
  return NULL;
}

void TabCaptureRegistry::DeleteCaptureRequest(int render_process_id,
                                              int render_view_id) {
  for (ScopedVector<TabCaptureRequest>::iterator it = requests_.begin();
       it != requests_.end(); ++it) {
    if ((*it)->render_process_id == render_process_id &&
        (*it)->render_view_id == render_view_id) {
      requests_.erase(it);
      return;
    }
  }
}

}  // namespace extensions
