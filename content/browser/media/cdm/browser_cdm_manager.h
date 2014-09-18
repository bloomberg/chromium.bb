// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CDM_BROWSER_CDM_MANAGER_H_
#define CONTENT_BROWSER_MEDIA_CDM_BROWSER_CDM_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "content/common/media/cdm_messages_enums.h"
#include "content/public/browser/browser_message_filter.h"
#include "ipc/ipc_message.h"
// TODO(xhwang): Drop this when KeyError is moved to a common header.
#include "media/base/media_keys.h"
#include "url/gurl.h"

namespace media {
class BrowserCdm;
}

namespace content {

// This class manages all CDM objects. It receives control operations from the
// the render process, and forwards them to corresponding CDM object. Callbacks
// from CDM objects are converted to IPCs and then sent to the render process.
class CONTENT_EXPORT BrowserCdmManager : public BrowserMessageFilter {
 public:
  // Returns the BrowserCdmManager associated with the |render_process_id|.
  // Returns NULL if no BrowserCdmManager is associated.
  static BrowserCdmManager* FromProcess(int render_process_id);

  // Constructs the BrowserCdmManager for |render_process_id| which runs on
  // |task_runner|.
  // If |task_runner| is not NULL, all CDM messages are posted to it. Otherwise,
  // all messages are posted to the browser UI thread.
  BrowserCdmManager(int render_process_id,
                    const scoped_refptr<base::TaskRunner>& task_runner);

  // BrowserMessageFilter implementations.
  virtual void OnDestruct() const OVERRIDE;
  virtual base::TaskRunner* OverrideTaskRunnerForMessage(
      const IPC::Message& message) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  media::BrowserCdm* GetCdm(int render_frame_id, int cdm_id);

  // Notifies that the render frame has been deleted so that all CDMs belongs
  // to this render frame needs to be destroyed as well. This is needed because
  // in some cases (e.g. fast termination of the renderer), the message to
  // destroy the CDM will not be received.
  void RenderFrameDeleted(int render_frame_id);

 protected:
  friend class base::RefCountedThreadSafe<BrowserCdmManager>;
  friend class base::DeleteHelper<BrowserCdmManager>;
  virtual ~BrowserCdmManager();

 private:
  // CDM callbacks.
  void OnSessionCreated(int render_frame_id,
                        int cdm_id,
                        uint32 session_id,
                        const std::string& web_session_id);
  void OnSessionMessage(int render_frame_id,
                        int cdm_id,
                        uint32 session_id,
                        const std::vector<uint8>& message,
                        const GURL& destination_url);
  void OnSessionReady(int render_frame_id, int cdm_id, uint32 session_id);
  void OnSessionClosed(int render_frame_id, int cdm_id, uint32 session_id);
  void OnSessionError(int render_frame_id,
                      int cdm_id,
                      uint32 session_id,
                      media::MediaKeys::KeyError error_code,
                      uint32 system_code);

  // Message handlers.
  void OnInitializeCdm(int render_frame_id,
                       int cdm_id,
                       const std::string& key_system,
                       const GURL& frame_url);
  void OnCreateSession(int render_frame_id,
                       int cdm_id,
                       uint32 session_id,
                       CdmHostMsg_CreateSession_ContentType content_type,
                       const std::vector<uint8>& init_data);
  void OnUpdateSession(int render_frame_id,
                       int cdm_id,
                       uint32 session_id,
                       const std::vector<uint8>& response);
  void OnReleaseSession(int render_frame_id,
                        int cdm_id, uint32 session_id);
  void OnDestroyCdm(int render_frame_id, int cdm_id);

  void SendSessionError(int render_frame_id, int cdm_id, uint32 session_id);

  // Adds a new CDM identified by |cdm_id| for the given |key_system| and
  // |security_origin|.
  void AddCdm(int render_frame_id,
              int cdm_id,
              const std::string& key_system,
              const GURL& security_origin);

  // Removes the CDM with the specified id.
  void RemoveCdm(uint64 id);

  // If |permitted| is false, it does nothing but send
  // |CdmMsg_SessionError| IPC message.
  // The primary use case is infobar permission callback, i.e., when infobar
  // can decide user's intention either from interacting with the actual info
  // bar or from the saved preference.
  void CreateSessionIfPermitted(int render_frame_id,
                                int cdm_id,
                                uint32 session_id,
                                const std::string& content_type,
                                const std::vector<uint8>& init_data,
                                bool permitted);

  const int render_process_id_;

  // TaskRunner to dispatch all CDM messages to. If it's NULL, all messages are
  // dispatched to the browser UI thread.
  scoped_refptr<base::TaskRunner> task_runner_;

  // The key in the following maps is a combination of |render_frame_id| and
  // |cdm_id|.

  // Map of managed BrowserCdms.
  typedef base::ScopedPtrHashMap<uint64, media::BrowserCdm> CdmMap;
  CdmMap cdm_map_;

  // Map of CDM's security origin.
  std::map<uint64, GURL> cdm_security_origin_map_;

  // Map of callbacks to cancel the permission request.
  std::map<uint64, base::Closure> cdm_cancel_permission_map_;

  DISALLOW_COPY_AND_ASSIGN(BrowserCdmManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CDM_BROWSER_CDM_MANAGER_H_
