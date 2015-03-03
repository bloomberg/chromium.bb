// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/cdm/browser_cdm_manager.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
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
#include "media/base/cdm_promise.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"

namespace content {

using media::BrowserCdm;
using media::MediaKeys;

namespace {

// Maximum lengths for various EME API parameters. These are checks to
// prevent unnecessarily large parameters from being passed around, and the
// lengths are somewhat arbitrary as the EME spec doesn't specify any limits.
const size_t kMaxInitDataLength = 64 * 1024;  // 64 KB
const size_t kMaxSessionResponseLength = 64 * 1024;  // 64 KB
const size_t kMaxKeySystemLength = 256;

// The ID used in this class is a concatenation of |render_frame_id| and
// |cdm_id|, i.e. (render_frame_id << 32) + cdm_id.

uint64 GetId(int render_frame_id, int cdm_id) {
  return (static_cast<uint64>(render_frame_id) << 32) +
         static_cast<uint64>(cdm_id);
}

bool IdBelongsToFrame(uint64 id, int render_frame_id) {
  return (id >> 32) == static_cast<uint64>(render_frame_id);
}

// media::CdmPromiseTemplate implementation backed by a BrowserCdmManager.
template <typename... T>
class CdmPromiseInternal : public media::CdmPromiseTemplate<T...> {
 public:
  CdmPromiseInternal(BrowserCdmManager* manager,
                     int render_frame_id,
                     int cdm_id,
                     uint32_t promise_id)
      : manager_(manager),
        render_frame_id_(render_frame_id),
        cdm_id_(cdm_id),
        promise_id_(promise_id) {
    DCHECK(manager_);
  }

  ~CdmPromiseInternal() final {}

  // CdmPromiseTemplate<> implementation.
  void resolve(const T&... result) final;

  void reject(MediaKeys::Exception exception,
              uint32_t system_code,
              const std::string& error_message) final {
    MarkPromiseSettled();
    manager_->RejectPromise(render_frame_id_, cdm_id_, promise_id_, exception,
                            system_code, error_message);
  }

 private:
  using media::CdmPromiseTemplate<T...>::MarkPromiseSettled;

  BrowserCdmManager* const manager_;
  const int render_frame_id_;
  const int cdm_id_;
  const uint32_t promise_id_;
};

template <>
void CdmPromiseInternal<>::resolve() {
  MarkPromiseSettled();
  manager_->ResolvePromise(render_frame_id_, cdm_id_, promise_id_);
}

template <>
void CdmPromiseInternal<std::string>::resolve(const std::string& session_id) {
  MarkPromiseSettled();
  manager_->ResolvePromiseWithSession(render_frame_id_, cdm_id_, promise_id_,
                                      session_id);
}

typedef CdmPromiseInternal<> SimplePromise;
typedef CdmPromiseInternal<std::string> NewSessionPromise;

}  // namespace

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
      task_runner_(task_runner),
      weak_ptr_factory_(this) {
  DVLOG(1) << __FUNCTION__ << ": " << render_process_id_;
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!task_runner_.get()) {
    task_runner_ =
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI);
  }

  DCHECK(!g_browser_cdm_manager_map.Get().count(render_process_id_))
      << render_process_id_;
  g_browser_cdm_manager_map.Get()[render_process_id] = this;
}

BrowserCdmManager::~BrowserCdmManager() {
  DVLOG(1) << __FUNCTION__ << ": " << render_process_id_;
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(g_browser_cdm_manager_map.Get().count(render_process_id_));
  DCHECK_EQ(this, g_browser_cdm_manager_map.Get()[render_process_id_]);

  g_browser_cdm_manager_map.Get().erase(render_process_id_);
}

// Makes sure BrowserCdmManager is always deleted on the Browser UI thread.
void BrowserCdmManager::OnDestruct() const {
  DVLOG(1) << __FUNCTION__ << ": " << render_process_id_;
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
    IPC_MESSAGE_HANDLER(CdmHostMsg_SetServerCertificate, OnSetServerCertificate)
    IPC_MESSAGE_HANDLER(CdmHostMsg_CreateSessionAndGenerateRequest,
                        OnCreateSessionAndGenerateRequest)
    IPC_MESSAGE_HANDLER(CdmHostMsg_UpdateSession, OnUpdateSession)
    IPC_MESSAGE_HANDLER(CdmHostMsg_CloseSession, OnCloseSession)
    IPC_MESSAGE_HANDLER(CdmHostMsg_DestroyCdm, OnDestroyCdm)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

media::BrowserCdm* BrowserCdmManager::GetCdm(int render_frame_id,
                                             int cdm_id) const {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  return cdm_map_.get(GetId(render_frame_id, cdm_id));
}

void BrowserCdmManager::RenderFrameDeleted(int render_frame_id) {
  if (!task_runner_->RunsTasksOnCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&BrowserCdmManager::RemoveAllCdmForFrame,
                   this, render_frame_id));
    return;
  }
  RemoveAllCdmForFrame(render_frame_id);
}

void BrowserCdmManager::ResolvePromise(int render_frame_id,
                                       int cdm_id,
                                       uint32_t promise_id) {
  Send(new CdmMsg_ResolvePromise(render_frame_id, cdm_id, promise_id));
}

void BrowserCdmManager::ResolvePromiseWithSession(
    int render_frame_id,
    int cdm_id,
    uint32_t promise_id,
    const std::string& session_id) {
  if (session_id.length() > media::limits::kMaxSessionIdLength) {
    RejectPromise(render_frame_id, cdm_id, promise_id,
                  MediaKeys::INVALID_ACCESS_ERROR, 0,
                  "Session ID is too long.");
    return;
  }

  Send(new CdmMsg_ResolvePromiseWithSession(render_frame_id, cdm_id, promise_id,
                                            session_id));
}

void BrowserCdmManager::RejectPromise(int render_frame_id,
                                      int cdm_id,
                                      uint32_t promise_id,
                                      media::MediaKeys::Exception exception,
                                      uint32_t system_code,
                                      const std::string& error_message) {
  Send(new CdmMsg_RejectPromise(render_frame_id, cdm_id, promise_id, exception,
                                system_code, error_message));
}

void BrowserCdmManager::OnSessionMessage(
    int render_frame_id,
    int cdm_id,
    const std::string& session_id,
    media::MediaKeys::MessageType message_type,
    const std::vector<uint8>& message,
    const GURL& legacy_destination_url) {
  GURL verified_gurl = legacy_destination_url;
  if (!verified_gurl.is_valid() && !verified_gurl.is_empty()) {
    DLOG(WARNING) << "SessionMessage legacy_destination_url is invalid : "
                  << legacy_destination_url.possibly_invalid_spec();
    verified_gurl =
        GURL::EmptyGURL();  // Replace invalid legacy_destination_url.
  }

  Send(new CdmMsg_SessionMessage(render_frame_id, cdm_id, session_id,
                                 message_type, message, verified_gurl));
}

void BrowserCdmManager::OnSessionClosed(int render_frame_id,
                                        int cdm_id,
                                        const std::string& session_id) {
  Send(new CdmMsg_SessionClosed(render_frame_id, cdm_id, session_id));
}

void BrowserCdmManager::OnLegacySessionError(
    int render_frame_id,
    int cdm_id,
    const std::string& session_id,
    MediaKeys::Exception exception_code,
    uint32 system_code,
    const std::string& error_message) {
  Send(new CdmMsg_LegacySessionError(render_frame_id, cdm_id, session_id,
                                     exception_code, system_code,
                                     error_message));
}

void BrowserCdmManager::OnSessionKeysChange(int render_frame_id,
                                            int cdm_id,
                                            const std::string& session_id,
                                            bool has_additional_usable_key,
                                            media::CdmKeysInfo keys_info) {
  std::vector<media::CdmKeyInformation> key_info_vector;
  for (const auto& key_info : keys_info)
    key_info_vector.push_back(*key_info);
  Send(new CdmMsg_SessionKeysChange(render_frame_id, cdm_id, session_id,
                                    has_additional_usable_key,
                                    key_info_vector));
}

void BrowserCdmManager::OnSessionExpirationUpdate(
    int render_frame_id,
    int cdm_id,
    const std::string& session_id,
    const base::Time& new_expiry_time) {
  Send(new CdmMsg_SessionExpirationUpdate(render_frame_id, cdm_id, session_id,
                                          new_expiry_time));
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

void BrowserCdmManager::OnSetServerCertificate(
    int render_frame_id,
    int cdm_id,
    uint32_t promise_id,
    const std::vector<uint8_t>& certificate) {
  scoped_ptr<SimplePromise> promise(
      new SimplePromise(this, render_frame_id, cdm_id, promise_id));

  BrowserCdm* cdm = GetCdm(render_frame_id, cdm_id);
  if (!cdm) {
    promise->reject(MediaKeys::INVALID_STATE_ERROR, 0, "CDM not found.");
    return;
  }

  if (certificate.empty()) {
    promise->reject(MediaKeys::INVALID_ACCESS_ERROR, 0, "Empty certificate.");
    return;
  }

  cdm->SetServerCertificate(&certificate[0], certificate.size(),
                            promise.Pass());
}

void BrowserCdmManager::OnCreateSessionAndGenerateRequest(
    int render_frame_id,
    int cdm_id,
    uint32_t promise_id,
    CdmHostMsg_CreateSession_InitDataType init_data_type,
    const std::vector<uint8>& init_data) {
  scoped_ptr<NewSessionPromise> promise(
      new NewSessionPromise(this, render_frame_id, cdm_id, promise_id));

  if (init_data.size() > kMaxInitDataLength) {
    LOG(WARNING) << "InitData for ID: " << cdm_id
                 << " too long: " << init_data.size();
    promise->reject(MediaKeys::INVALID_ACCESS_ERROR, 0, "Init data too long.");
    return;
  }

  media::EmeInitDataType eme_init_data_type = media::EME_INIT_DATA_TYPE_NONE;
  switch (init_data_type) {
    case INIT_DATA_TYPE_WEBM:
      eme_init_data_type = media::EME_INIT_DATA_TYPE_WEBM;
      break;
#if defined(USE_PROPRIETARY_CODECS)
    case INIT_DATA_TYPE_CENC:
      eme_init_data_type = media::EME_INIT_DATA_TYPE_CENC;
      break;
#endif
    default:
      NOTREACHED();
      promise->reject(MediaKeys::INVALID_ACCESS_ERROR, 0,
                      "Invalid init data type.");
      return;
  }

#if defined(OS_ANDROID)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableInfobarForProtectedMediaIdentifier)) {
    GenerateRequestIfPermitted(
        render_frame_id, cdm_id, eme_init_data_type,
        init_data, promise.Pass(), PERMISSION_STATUS_GRANTED);
    return;
  }
#endif

  BrowserCdm* cdm = GetCdm(render_frame_id, cdm_id);
  if (!cdm) {
    DLOG(WARNING) << "No CDM found for: " << render_frame_id << ", " << cdm_id;
    promise->reject(MediaKeys::INVALID_STATE_ERROR, 0, "CDM not found.");
    return;
  }

  std::map<uint64, GURL>::const_iterator iter =
      cdm_security_origin_map_.find(GetId(render_frame_id, cdm_id));
  if (iter == cdm_security_origin_map_.end()) {
    NOTREACHED();
    promise->reject(MediaKeys::INVALID_STATE_ERROR, 0, "CDM not found.");
    return;
  }
  GURL security_origin = iter->second;

  RequestSessionPermission(render_frame_id, security_origin, cdm_id,
                           eme_init_data_type, init_data, promise.Pass());
}

void BrowserCdmManager::OnUpdateSession(int render_frame_id,
                                        int cdm_id,
                                        uint32_t promise_id,
                                        const std::string& session_id,
                                        const std::vector<uint8>& response) {
  scoped_ptr<SimplePromise> promise(
      new SimplePromise(this, render_frame_id, cdm_id, promise_id));

  BrowserCdm* cdm = GetCdm(render_frame_id, cdm_id);
  if (!cdm) {
    promise->reject(MediaKeys::INVALID_STATE_ERROR, 0, "CDM not found.");
    return;
  }

  if (response.size() > kMaxSessionResponseLength) {
    LOG(WARNING) << "Response for ID " << cdm_id
                 << " is too long: " << response.size();
    promise->reject(MediaKeys::INVALID_ACCESS_ERROR, 0, "Response too long.");
    return;
  }

  if (response.empty()) {
    promise->reject(MediaKeys::INVALID_ACCESS_ERROR, 0, "Response is empty.");
    return;
  }

  cdm->UpdateSession(session_id, &response[0], response.size(), promise.Pass());
}

void BrowserCdmManager::OnCloseSession(int render_frame_id,
                                       int cdm_id,
                                       uint32_t promise_id,
                                       const std::string& session_id) {
  scoped_ptr<SimplePromise> promise(
      new SimplePromise(this, render_frame_id, cdm_id, promise_id));

  BrowserCdm* cdm = GetCdm(render_frame_id, cdm_id);
  if (!cdm) {
    promise->reject(MediaKeys::INVALID_STATE_ERROR, 0, "CDM not found.");
    return;
  }

  cdm->CloseSession(session_id, promise.Pass());
}

void BrowserCdmManager::OnDestroyCdm(int render_frame_id, int cdm_id) {
  RemoveCdm(GetId(render_frame_id, cdm_id));
}

// Use a weak pointer here instead of |this| to avoid circular references.
#define BROWSER_CDM_MANAGER_CB(func)                                   \
  base::Bind(&BrowserCdmManager::func, weak_ptr_factory_.GetWeakPtr(), \
             render_frame_id, cdm_id)

void BrowserCdmManager::AddCdm(int render_frame_id,
                               int cdm_id,
                               const std::string& key_system,
                               const GURL& security_origin) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!GetCdm(render_frame_id, cdm_id));

  scoped_ptr<BrowserCdm> cdm(media::CreateBrowserCdm(
      key_system, BROWSER_CDM_MANAGER_CB(OnSessionMessage),
      BROWSER_CDM_MANAGER_CB(OnSessionClosed),
      BROWSER_CDM_MANAGER_CB(OnLegacySessionError),
      BROWSER_CDM_MANAGER_CB(OnSessionKeysChange),
      BROWSER_CDM_MANAGER_CB(OnSessionExpirationUpdate)));

  if (!cdm) {
    // This failure will be discovered and reported by
    // OnCreateSessionAndGenerateRequest() as GetCdm() will return null.
    DVLOG(1) << "failed to create CDM.";
    return;
  }

  uint64 id = GetId(render_frame_id, cdm_id);
  cdm_map_.add(id, cdm.Pass());
  cdm_security_origin_map_[id] = security_origin;
}

void BrowserCdmManager::RemoveAllCdmForFrame(int render_frame_id) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  std::vector<uint64> ids_to_remove;
  for (CdmMap::iterator it = cdm_map_.begin(); it != cdm_map_.end(); ++it) {
    if (IdBelongsToFrame(it->first, render_frame_id))
      ids_to_remove.push_back(it->first);
  }

  for (size_t i = 0; i < ids_to_remove.size(); ++i)
    RemoveCdm(ids_to_remove[i]);
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

void BrowserCdmManager::RequestSessionPermission(
    int render_frame_id,
    const GURL& security_origin,
    int cdm_id,
    media::EmeInitDataType init_data_type,
    const std::vector<uint8>& init_data,
    scoped_ptr<media::NewSessionCdmPromise> promise) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&BrowserCdmManager::RequestSessionPermission, this,
                   render_frame_id, security_origin, cdm_id, init_data_type,
                   init_data, base::Passed(&promise)));
    return;
  }

  RenderFrameHost* rfh =
      RenderFrameHost::FromID(render_process_id_, render_frame_id);
  WebContents* web_contents = WebContents::FromRenderFrameHost(rfh);
  DCHECK(web_contents);
  GetContentClient()->browser()->RequestPermission(
      content::PERMISSION_PROTECTED_MEDIA_IDENTIFIER, web_contents,
      0,  // bridge id
      security_origin,
      // Only implemented for Android infobars which do not support
      // user gestures.
      true, base::Bind(&BrowserCdmManager::GenerateRequestIfPermitted, this,
                       render_frame_id, cdm_id, init_data_type, init_data,
                       base::Passed(&promise)));
}

void BrowserCdmManager::GenerateRequestIfPermitted(
    int render_frame_id,
    int cdm_id,
    media::EmeInitDataType init_data_type,
    const std::vector<uint8>& init_data,
    scoped_ptr<media::NewSessionCdmPromise> promise,
    PermissionStatus permission) {
  cdm_cancel_permission_map_.erase(GetId(render_frame_id, cdm_id));
  if (permission != PERMISSION_STATUS_GRANTED) {
    promise->reject(MediaKeys::NOT_SUPPORTED_ERROR, 0, "Permission denied.");
    return;
  }

  BrowserCdm* cdm = GetCdm(render_frame_id, cdm_id);
  if (!cdm) {
    promise->reject(MediaKeys::INVALID_STATE_ERROR, 0, "CDM not found.");
    return;
  }

  // TODO(ddorwin): Move this conversion to MediaDrmBridge when fixing
  // crbug.com/417440.
  // "audio"/"video" does not matter, so use "video".
  std::string init_data_type_string;
  switch (init_data_type) {
    case media::EME_INIT_DATA_TYPE_WEBM:
      init_data_type_string = "video/webm";
      break;
#if defined(USE_PROPRIETARY_CODECS)
    case media::EME_INIT_DATA_TYPE_CENC:
      init_data_type_string = "video/mp4";
      break;
#endif
    default:
      NOTREACHED();
  }

  // Only the temporary session type is supported in browser CDM path.
  // TODO(xhwang): Add SessionType support if needed.
  cdm->CreateSessionAndGenerateRequest(media::MediaKeys::TEMPORARY_SESSION,
                                       init_data_type_string, &init_data[0],
                                       init_data.size(), promise.Pass());
}

}  // namespace content
