// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_ANDROID_PROXY_MEDIA_KEYS_H_
#define CONTENT_RENDERER_MEDIA_ANDROID_PROXY_MEDIA_KEYS_H_

#include "base/basictypes.h"
#include "media/base/media_keys.h"

class GURL;

namespace content {

class RendererMediaPlayerManager;

// A MediaKeys proxy that wraps the EME part of RendererMediaPlayerManager.
// TODO(xhwang): Instead of accessing RendererMediaPlayerManager directly, let
// RendererMediaPlayerManager return a MediaKeys object that can be used by
// ProxyDecryptor directly. Then we can remove this class!
class ProxyMediaKeys : public media::MediaKeys {
 public:
  ProxyMediaKeys(RendererMediaPlayerManager* proxy,
                 int media_keys_id,
                 const media::SessionCreatedCB& session_created_cb,
                 const media::SessionMessageCB& session_message_cb,
                 const media::SessionReadyCB& session_ready_cb,
                 const media::SessionClosedCB& session_closed_cb,
                 const media::SessionErrorCB& session_error_cb);
  virtual ~ProxyMediaKeys();

  void InitializeCDM(const std::string& key_system, const GURL& frame_url);

  // MediaKeys implementation.
  virtual bool CreateSession(uint32 session_id,
                             const std::string& type,
                             const uint8* init_data,
                             int init_data_length) OVERRIDE;
  virtual void UpdateSession(uint32 session_id,
                             const uint8* response,
                             int response_length) OVERRIDE;
  virtual void ReleaseSession(uint32 session_id) OVERRIDE;

  // Callbacks.
  void OnSessionCreated(uint32 session_id, const std::string& web_session_id);
  void OnSessionMessage(uint32 session_id,
                        const std::vector<uint8>& message,
                        const std::string& destination_url);
  void OnSessionReady(uint32 session_id);
  void OnSessionClosed(uint32 session_id);
  void OnSessionError(uint32 session_id,
                      media::MediaKeys::KeyError error_code,
                      int system_code);

 private:
  RendererMediaPlayerManager* manager_;
  int media_keys_id_;
  media::SessionCreatedCB session_created_cb_;
  media::SessionMessageCB session_message_cb_;
  media::SessionReadyCB session_ready_cb_;
  media::SessionClosedCB session_closed_cb_;
  media::SessionErrorCB session_error_cb_;

  DISALLOW_COPY_AND_ASSIGN(ProxyMediaKeys);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_ANDROID_PROXY_MEDIA_KEYS_H_
