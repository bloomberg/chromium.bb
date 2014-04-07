// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/proxy_media_keys.h"

#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "content/renderer/media/android/renderer_media_player_manager.h"
#include "content/renderer/media/crypto/key_systems.h"

namespace content {

int ProxyMediaKeys::next_cdm_id_ =
    RendererMediaPlayerManager::kInvalidCdmId + 1;

scoped_ptr<ProxyMediaKeys> ProxyMediaKeys::Create(
    const std::string& key_system,
    const GURL& security_origin,
    RendererMediaPlayerManager* manager,
    const media::SessionCreatedCB& session_created_cb,
    const media::SessionMessageCB& session_message_cb,
    const media::SessionReadyCB& session_ready_cb,
    const media::SessionClosedCB& session_closed_cb,
    const media::SessionErrorCB& session_error_cb) {
  DCHECK(manager);
  scoped_ptr<ProxyMediaKeys> proxy_media_keys(
      new ProxyMediaKeys(manager,
                         session_created_cb,
                         session_message_cb,
                         session_ready_cb,
                         session_closed_cb,
                         session_error_cb));
  proxy_media_keys->InitializeCdm(key_system, security_origin);
  return proxy_media_keys.Pass();
}

ProxyMediaKeys::~ProxyMediaKeys() {
  manager_->DestroyCdm(cdm_id_);
}

bool ProxyMediaKeys::CreateSession(uint32 session_id,
                                   const std::string& content_type,
                                   const uint8* init_data,
                                   int init_data_length) {
  // TODO(xhwang): Move these checks up to blink and DCHECK here.
  // See http://crbug.com/342510
  CdmHostMsg_CreateSession_ContentType session_type;
  if (content_type == "audio/mp4" || content_type == "video/mp4") {
    session_type = CREATE_SESSION_TYPE_MP4;
  } else if (content_type == "audio/webm" || content_type == "video/webm") {
    session_type = CREATE_SESSION_TYPE_WEBM;
  } else {
    DLOG(ERROR) << "Unsupported EME CreateSession content type of "
                << content_type;
    return false;
  }

  manager_->CreateSession(
      cdm_id_,
      session_id,
      session_type,
      std::vector<uint8>(init_data, init_data + init_data_length));
  return true;
}

void ProxyMediaKeys::LoadSession(uint32 session_id,
                                 const std::string& web_session_id) {
  // TODO(xhwang): Check key system and platform support for LoadSession in
  // blink and add NOTREACHED() here.
  DLOG(ERROR) << "ProxyMediaKeys doesn't support session loading.";
  OnSessionError(session_id, media::MediaKeys::kUnknownError, 0);
}

void ProxyMediaKeys::UpdateSession(uint32 session_id,
                                   const uint8* response,
                                   int response_length) {
  manager_->UpdateSession(
      cdm_id_,
      session_id,
      std::vector<uint8>(response, response + response_length));
}

void ProxyMediaKeys::ReleaseSession(uint32 session_id) {
  manager_->ReleaseSession(cdm_id_, session_id);
}

void ProxyMediaKeys::OnSessionCreated(uint32 session_id,
                                      const std::string& web_session_id) {
  session_created_cb_.Run(session_id, web_session_id);
}

void ProxyMediaKeys::OnSessionMessage(uint32 session_id,
                                      const std::vector<uint8>& message,
                                      const std::string& destination_url) {
  session_message_cb_.Run(session_id, message, destination_url);
}

void ProxyMediaKeys::OnSessionReady(uint32 session_id) {
  session_ready_cb_.Run(session_id);
}

void ProxyMediaKeys::OnSessionClosed(uint32 session_id) {
  session_closed_cb_.Run(session_id);
}

void ProxyMediaKeys::OnSessionError(uint32 session_id,
                                    media::MediaKeys::KeyError error_code,
                                    uint32 system_code) {
  session_error_cb_.Run(session_id, error_code, system_code);
}

int ProxyMediaKeys::GetCdmId() const {
  return cdm_id_;
}

ProxyMediaKeys::ProxyMediaKeys(
    RendererMediaPlayerManager* manager,
    const media::SessionCreatedCB& session_created_cb,
    const media::SessionMessageCB& session_message_cb,
    const media::SessionReadyCB& session_ready_cb,
    const media::SessionClosedCB& session_closed_cb,
    const media::SessionErrorCB& session_error_cb)
    : manager_(manager),
      cdm_id_(next_cdm_id_++),
      session_created_cb_(session_created_cb),
      session_message_cb_(session_message_cb),
      session_ready_cb_(session_ready_cb),
      session_closed_cb_(session_closed_cb),
      session_error_cb_(session_error_cb) {
}

void ProxyMediaKeys::InitializeCdm(const std::string& key_system,
                                   const GURL& security_origin) {
  manager_->InitializeCdm(cdm_id_, this, key_system, security_origin);
}

}  // namespace content
