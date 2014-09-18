// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/cdm/browser_cdm_manager.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/task_runner.h"
#include "content/common/media/cdm_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "media/base/browser_cdm.h"
#include "media/base/browser_cdm_factory.h"
#include "media/base/media_switches.h"

namespace content {

using media::BrowserCdm;
using media::MediaKeys;

// Maximum lengths for various EME API parameters. These are checks to
// prevent unnecessarily large parameters from being passed around, and the
// lengths are somewhat arbitrary as the EME spec doesn't specify any limits.
const size_t kMaxInitDataLength = 64 * 1024;  // 64 KB
const size_t kMaxSessionResponseLength = 64 * 1024;  // 64 KB
const size_t kMaxKeySystemLength = 256;

// The ID used in this class is a concatenation of |render_frame_id| and
// |cdm_id|, i.e. (render_frame_id << 32) + cdm_id.

static uint64 GetId(int render_frame_id, int cdm_id) {
  return (static_cast<uint64>(render_frame_id) << 32) +
         static_cast<uint64>(cdm_id);
}

static bool IdBelongsToFrame(uint64 id, int render_frame_id) {
  return (id >> 32) == static_cast<uint64>(render_frame_id);
}

// Render process ID to BrowserCdmManager map.
typedef std::map<int, BrowserCdmManager*> BrowserCdmManagerMap;
base::LazyInstance<BrowserCdmManagerMap> g_browser_cdm_manager_map =
    LAZY_INSTANCE_INITIALIZER;

BrowserCdmManager* BrowserCdmManager::FromProcess(int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!g_browser_cdm_manager_map.Get().count(render_process_id))
    return NULL;

  return g_browser_cdm_manager_map.Get()[render_process_id];
}

BrowserCdmManager::BrowserCdmManager(
    int render_process_id,
    const scoped_refptr<base::TaskRunner>& task_runner)
    : BrowserMessageFilter(CdmMsgStart),
      render_process_id_(render_process_id),
      task_runner_(task_runner) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!task_runner_) {
    task_runner_ =
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI);
  }

  // This may overwrite an existing entry of |render_process_id| if the
  // previous process crashed and didn't cleanup its child frames. For example,
  // see FrameTreeBrowserTest.FrameTreeAfterCrash test.
  g_browser_cdm_manager_map.Get()[render_process_id] = this;
}

BrowserCdmManager::~BrowserCdmManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(g_browser_cdm_manager_map.Get().count(render_process_id_));

  g_browser_cdm_manager_map.Get().erase(render_process_id_);
}

// Makes sure BrowserCdmManager is always deleted on the Browser UI thread.
void BrowserCdmManager::OnDestruct() const {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    delete this;
  } else {
    BrowserThread::DeleteSoon(BrowserThread::UI, FROM_HERE, this);
  }
}

base::TaskRunner* BrowserCdmManager::OverrideTaskRunnerForMessage(
    const IPC::Message& message) {
  // Only handles CDM messages.
  if (IPC_MESSAGE_CLASS(message) != CdmMsgStart)
    return NULL;

  return task_runner_.get();
}

bool BrowserCdmManager::OnMessageReceived(const IPC::Message& msg) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BrowserCdmManager, msg)
    IPC_MESSAGE_HANDLER(CdmHostMsg_InitializeCdm, OnInitializeCdm)
    IPC_MESSAGE_HANDLER(CdmHostMsg_CreateSession, OnCreateSession)
    IPC_MESSAGE_HANDLER(CdmHostMsg_UpdateSession, OnUpdateSession)
    IPC_MESSAGE_HANDLER(CdmHostMsg_ReleaseSession, OnReleaseSession)
    IPC_MESSAGE_HANDLER(CdmHostMsg_DestroyCdm, OnDestroyCdm)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

media::BrowserCdm* BrowserCdmManager::GetCdm(int render_frame_id, int cdm_id) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  return cdm_map_.get(GetId(render_frame_id, cdm_id));
}

void BrowserCdmManager::RenderFrameDeleted(int render_frame_id) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  std::vector<uint64> ids_to_remove;
  for (CdmMap::iterator it = cdm_map_.begin(); it != cdm_map_.end(); ++it) {
    if (IdBelongsToFrame(it->first, render_frame_id))
      ids_to_remove.push_back(it->first);
  }

  for (size_t i = 0; i < ids_to_remove.size(); ++i)
    RemoveCdm(ids_to_remove[i]);
}

void BrowserCdmManager::OnSessionCreated(int render_frame_id,
                                         int cdm_id,
                                         uint32 session_id,
                                         const std::string& web_session_id) {
  Send(new CdmMsg_SessionCreated(
      render_frame_id, cdm_id, session_id, web_session_id));
}

void BrowserCdmManager::OnSessionMessage(int render_frame_id,
                                         int cdm_id,
                                         uint32 session_id,
                                         const std::vector<uint8>& message,
                                         const GURL& destination_url) {
  GURL verified_gurl = destination_url;
  if (!verified_gurl.is_valid() && !verified_gurl.is_empty()) {
    DLOG(WARNING) << "SessionMessage destination_url is invalid : "
                  << destination_url.possibly_invalid_spec();
    verified_gurl = GURL::EmptyGURL();  // Replace invalid destination_url.
  }

  Send(new CdmMsg_SessionMessage(
      render_frame_id, cdm_id, session_id, message, verified_gurl));
}

void BrowserCdmManager::OnSessionReady(int render_frame_id,
                                       int cdm_id,
                                       uint32 session_id) {
  Send(new CdmMsg_SessionReady(render_frame_id, cdm_id, session_id));
}

void BrowserCdmManager::OnSessionClosed(int render_frame_id,
                                        int cdm_id,
                                        uint32 session_id) {
  Send(new CdmMsg_SessionClosed(render_frame_id, cdm_id, session_id));
}

void BrowserCdmManager::OnSessionError(int render_frame_id,
                                       int cdm_id,
                                       uint32 session_id,
                                       MediaKeys::KeyError error_code,
                                       uint32 system_code) {
  Send(new CdmMsg_SessionError(
      render_frame_id, cdm_id, session_id, error_code, system_code));
}

void BrowserCdmManager::OnInitializeCdm(int render_frame_id,
                                        int cdm_id,
                                        const std::string& key_system,
                                        const GURL& security_origin) {
  if (key_system.size() > kMaxKeySystemLength) {
    // This failure will be discovered and reported by OnCreateSession()
    // as GetCdm() will return null.
    NOTREACHED() << "Invalid key system: " << key_system;
    return;
  }

  AddCdm(render_frame_id, cdm_id, key_system, security_origin);
}

void BrowserCdmManager::OnCreateSession(
    int render_frame_id,
    int cdm_id,
    uint32 session_id,
    CdmHostMsg_CreateSession_ContentType content_type,
    const std::vector<uint8>& init_data) {
  if (init_data.size() > kMaxInitDataLength) {
    LOG(WARNING) << "InitData for ID: " << cdm_id
                 << " too long: " << init_data.size();
    SendSessionError(render_frame_id, cdm_id, session_id);
    return;
  }

  // Convert the session content type into a MIME type. "audio" and "video"
  // don't matter, so using "video" for the MIME type.
  // Ref:
  // https://dvcs.w3.org/hg/html-media/raw-file/default/encrypted-media/encrypted-media.html#dom-createsession
  std::string mime_type;
  switch (content_type) {
    case CREATE_SESSION_TYPE_WEBM:
      mime_type = "video/webm";
      break;
    case CREATE_SESSION_TYPE_MP4:
      mime_type = "video/mp4";
      break;
    default:
      NOTREACHED();
      return;
  }

#if defined(OS_ANDROID)
  if (CommandLine::ForCurrentProcess()
      ->HasSwitch(switches::kDisableInfobarForProtectedMediaIdentifier)) {
    CreateSessionIfPermitted(
        render_frame_id, cdm_id, session_id, mime_type, init_data, true);
    return;
  }
#endif

  BrowserCdm* cdm = GetCdm(render_frame_id, cdm_id);
  if (!cdm) {
    DLOG(WARNING) << "No CDM found for: " << render_frame_id << ", " << cdm_id;
    SendSessionError(render_frame_id, cdm_id, session_id);
    return;
  }

  std::map<uint64, GURL>::const_iterator iter =
      cdm_security_origin_map_.find(GetId(render_frame_id, cdm_id));
  if (iter == cdm_security_origin_map_.end()) {
    NOTREACHED();
    SendSessionError(render_frame_id, cdm_id, session_id);
    return;
  }
  GURL security_origin = iter->second;

  RenderFrameHost* rfh =
      RenderFrameHost::FromID(render_process_id_, render_frame_id);
  WebContents* web_contents = WebContents::FromRenderFrameHost(rfh);
  DCHECK(web_contents);

  base::Closure cancel_callback;
  GetContentClient()->browser()->RequestProtectedMediaIdentifierPermission(
      web_contents,
      security_origin,
      base::Bind(&BrowserCdmManager::CreateSessionIfPermitted,
                 this,
                 render_frame_id, cdm_id, session_id,
                 mime_type, init_data),
      &cancel_callback);

  if (cancel_callback.is_null())
    return;
  cdm_cancel_permission_map_[GetId(render_frame_id, cdm_id)] = cancel_callback;
}

void BrowserCdmManager::OnUpdateSession(
    int render_frame_id,
    int cdm_id,
    uint32 session_id,
    const std::vector<uint8>& response) {
  BrowserCdm* cdm = GetCdm(render_frame_id, cdm_id);
  if (!cdm) {
    DLOG(WARNING) << "No CDM found for: " << render_frame_id << ", " << cdm_id;
    SendSessionError(render_frame_id, cdm_id, session_id);
    return;
  }

  if (response.size() > kMaxSessionResponseLength) {
    LOG(WARNING) << "Response for ID " << cdm_id
                 << " is too long: " << response.size();
    SendSessionError(render_frame_id, cdm_id, session_id);
    return;
  }

  cdm->UpdateSession(session_id, &response[0], response.size());
}

void BrowserCdmManager::OnReleaseSession(int render_frame_id,
                                         int cdm_id,
                                         uint32 session_id) {
  BrowserCdm* cdm = GetCdm(render_frame_id, cdm_id);
  if (!cdm) {
    DLOG(WARNING) << "No CDM found for: " << render_frame_id << ", " << cdm_id;
    SendSessionError(render_frame_id, cdm_id, session_id);
    return;
  }

  cdm->ReleaseSession(session_id);
}

void BrowserCdmManager::OnDestroyCdm(int render_frame_id, int cdm_id) {
  RemoveCdm(GetId(render_frame_id, cdm_id));
}

void BrowserCdmManager::SendSessionError(int render_frame_id,
                                         int cdm_id,
                                         uint32 session_id) {
  OnSessionError(
        render_frame_id, cdm_id, session_id, MediaKeys::kUnknownError, 0);
}

#define BROWSER_CDM_MANAGER_CB(func) \
  base::Bind(&BrowserCdmManager::func, this, render_frame_id, cdm_id)

void BrowserCdmManager::AddCdm(int render_frame_id,
                               int cdm_id,
                               const std::string& key_system,
                               const GURL& security_origin) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!GetCdm(render_frame_id, cdm_id));

  scoped_ptr<BrowserCdm> cdm(
      media::CreateBrowserCdm(key_system,
                              BROWSER_CDM_MANAGER_CB(OnSessionCreated),
                              BROWSER_CDM_MANAGER_CB(OnSessionMessage),
                              BROWSER_CDM_MANAGER_CB(OnSessionReady),
                              BROWSER_CDM_MANAGER_CB(OnSessionClosed),
                              BROWSER_CDM_MANAGER_CB(OnSessionError)));

  if (!cdm) {
    // This failure will be discovered and reported by OnCreateSession()
    // as GetCdm() will return null.
    DVLOG(1) << "failed to create CDM.";
    return;
  }

  uint64 id = GetId(render_frame_id, cdm_id);
  cdm_map_.add(id, cdm.Pass());
  cdm_security_origin_map_[id] = security_origin;
}

void BrowserCdmManager::RemoveCdm(uint64 id) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  cdm_map_.erase(id);
  cdm_security_origin_map_.erase(id);
  if (cdm_cancel_permission_map_.count(id)) {
    cdm_cancel_permission_map_[id].Run();
    cdm_cancel_permission_map_.erase(id);
  }
}

void BrowserCdmManager::CreateSessionIfPermitted(
    int render_frame_id,
    int cdm_id,
    uint32 session_id,
    const std::string& content_type,
    const std::vector<uint8>& init_data,
    bool permitted) {
  cdm_cancel_permission_map_.erase(GetId(render_frame_id, cdm_id));
  if (!permitted) {
    SendSessionError(render_frame_id, cdm_id, session_id);
    return;
  }

  BrowserCdm* cdm = GetCdm(render_frame_id, cdm_id);
  if (!cdm) {
    DLOG(WARNING) << "No CDM found for: " << render_frame_id << ", " << cdm_id;
    SendSessionError(render_frame_id, cdm_id, session_id);
    return;
  }

  // This could fail, in which case a SessionError will be fired.
  cdm->CreateSession(session_id, content_type, &init_data[0], init_data.size());
}

}  // namespace content
