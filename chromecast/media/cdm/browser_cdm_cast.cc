// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cdm/browser_cdm_cast.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "media/base/cdm_key_information.h"
#include "media/base/cdm_promise.h"
#include "media/cdm/player_tracker_impl.h"

namespace chromecast {
namespace media {

BrowserCdmCast::BrowserCdmCast() {
  thread_checker_.DetachFromThread();
}

BrowserCdmCast::~BrowserCdmCast() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(player_tracker_impl_.get());
  player_tracker_impl_->NotifyCdmUnset();
}

void BrowserCdmCast::Initialize(
    const ::media::SessionMessageCB& session_message_cb,
    const ::media::SessionClosedCB& session_closed_cb,
    const ::media::LegacySessionErrorCB& legacy_session_error_cb,
    const ::media::SessionKeysChangeCB& session_keys_change_cb,
    const ::media::SessionExpirationUpdateCB& session_expiration_update_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());

  player_tracker_impl_.reset(new ::media::PlayerTrackerImpl);

  session_message_cb_ = session_message_cb;
  session_closed_cb_ = session_closed_cb;
  legacy_session_error_cb_ = legacy_session_error_cb;
  session_keys_change_cb_ = session_keys_change_cb;
  session_expiration_update_cb_ = session_expiration_update_cb;
}

int BrowserCdmCast::RegisterPlayer(const base::Closure& new_key_cb,
                                   const base::Closure& cdm_unset_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return player_tracker_impl_->RegisterPlayer(new_key_cb, cdm_unset_cb);
}

void BrowserCdmCast::UnregisterPlayer(int registration_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  player_tracker_impl_->UnregisterPlayer(registration_id);
}

void BrowserCdmCast::LoadSession(
    ::media::MediaKeys::SessionType session_type,
    const std::string& session_id,
    scoped_ptr<::media::NewSessionCdmPromise> promise) {
  NOTREACHED() << "LoadSession not supported";
  legacy_session_error_cb_.Run(
      session_id, ::media::MediaKeys::Exception::NOT_SUPPORTED_ERROR, 0,
      std::string());
}

::media::CdmContext* BrowserCdmCast::GetCdmContext() {
  NOTREACHED();
  return nullptr;
}

void BrowserCdmCast::OnSessionMessage(const std::string& session_id,
                                      const std::vector<uint8>& message,
                                      const GURL& destination_url) {
  // Note: Message type is not supported in Chromecast. Do our best guess here.
  ::media::MediaKeys::MessageType message_type =
      destination_url.is_empty() ? ::media::MediaKeys::LICENSE_REQUEST
                                 : ::media::MediaKeys::LICENSE_RENEWAL;
  session_message_cb_.Run(session_id,
                          message_type,
                          message,
                          destination_url);
}

void BrowserCdmCast::OnSessionClosed(const std::string& session_id) {
  session_closed_cb_.Run(session_id);
}

void BrowserCdmCast::OnSessionKeysChange(
    const std::string& session_id,
    const ::media::KeyIdAndKeyPairs& keys) {
  ::media::CdmKeysInfo cdm_keys_info;
  for (const std::pair<std::string, std::string>& key : keys) {
    scoped_ptr< ::media::CdmKeyInformation> cdm_key_information(
        new ::media::CdmKeyInformation());
    cdm_key_information->key_id.assign(key.first.begin(), key.first.end());
    cdm_keys_info.push_back(cdm_key_information.release());
  }
  session_keys_change_cb_.Run(session_id, true, cdm_keys_info.Pass());

  player_tracker_impl_->NotifyNewKey();
}

void BrowserCdmCast::SetServerCertificateHelper(
    const std::vector<uint8>& certificate_data,
    scoped_ptr<::media::SimpleCdmPromise> promise) {
  SetServerCertificate(certificate_data.data(),
                       certificate_data.size(),
                       promise.Pass());
}

void BrowserCdmCast::CreateSessionAndGenerateRequestHelper(
    ::media::MediaKeys::SessionType session_type,
    ::media::EmeInitDataType init_data_type,
    const std::vector<uint8>& init_data,
    scoped_ptr<::media::NewSessionCdmPromise> promise) {
  CreateSessionAndGenerateRequest(
      session_type, init_data_type, init_data.data(), init_data.size(),
      promise.Pass());
}

void BrowserCdmCast::UpdateSessionHelper(
    const std::string& session_id,
    const std::vector<uint8>& response,
    scoped_ptr<::media::SimpleCdmPromise> promise) {
  UpdateSession(session_id, response.data(), response.size(), promise.Pass());
}

// A macro runs current member function on |cdm_loop_| thread.
#define FORWARD_ON_CDM_THREAD(param_fn, ...) \
  cdm_loop_->PostTask( \
      FROM_HERE, \
      base::Bind(&BrowserCdmCast::param_fn, \
                 base::Unretained(browser_cdm_cast_.get()), ##__VA_ARGS__))


BrowserCdmCastUi::BrowserCdmCastUi(
    scoped_ptr<BrowserCdmCast> browser_cdm_cast,
    const scoped_refptr<base::MessageLoopProxy>& cdm_loop)
    : browser_cdm_cast_(browser_cdm_cast.Pass()),
      cdm_loop_(cdm_loop) {
}

BrowserCdmCastUi::~BrowserCdmCastUi() {
  DCHECK(thread_checker_.CalledOnValidThread());
  cdm_loop_->DeleteSoon(FROM_HERE, browser_cdm_cast_.release());
}

int BrowserCdmCastUi::RegisterPlayer(const base::Closure& new_key_cb,
                                     const base::Closure& cdm_unset_cb) {
  NOTREACHED() << "RegisterPlayer should be called on BrowserCdmCast";
  return -1;
}

void BrowserCdmCastUi::UnregisterPlayer(int registration_id) {
  NOTREACHED() << "UnregisterPlayer should be called on BrowserCdmCast";
}

BrowserCdmCast* BrowserCdmCastUi::browser_cdm_cast() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return browser_cdm_cast_.get();
}

void BrowserCdmCastUi::SetServerCertificate(
    const uint8* certificate_data,
    int certificate_data_length,
    scoped_ptr<::media::SimpleCdmPromise> promise) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FORWARD_ON_CDM_THREAD(
      SetServerCertificateHelper,
      std::vector<uint8>(certificate_data,
                         certificate_data + certificate_data_length),
      base::Passed(&promise));
}

void BrowserCdmCastUi::CreateSessionAndGenerateRequest(
    ::media::MediaKeys::SessionType session_type,
    ::media::EmeInitDataType init_data_type,
    const uint8* init_data,
    int init_data_length,
    scoped_ptr<::media::NewSessionCdmPromise> promise) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FORWARD_ON_CDM_THREAD(
      CreateSessionAndGenerateRequestHelper,
      session_type,
      init_data_type,
      std::vector<uint8>(init_data, init_data + init_data_length),
      base::Passed(&promise));
}

void BrowserCdmCastUi::LoadSession(
    ::media::MediaKeys::SessionType session_type,
    const std::string& session_id,
    scoped_ptr<::media::NewSessionCdmPromise> promise) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FORWARD_ON_CDM_THREAD(
      LoadSession, session_type, session_id, base::Passed(&promise));
}

void BrowserCdmCastUi::UpdateSession(
    const std::string& session_id,
    const uint8* response,
    int response_length,
    scoped_ptr<::media::SimpleCdmPromise> promise) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FORWARD_ON_CDM_THREAD(
      UpdateSessionHelper,
      session_id,
      std::vector<uint8>(response, response + response_length),
      base::Passed(&promise));
}

void BrowserCdmCastUi::CloseSession(
    const std::string& session_id,
    scoped_ptr<::media::SimpleCdmPromise> promise) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FORWARD_ON_CDM_THREAD(CloseSession, session_id, base::Passed(&promise));
}

void BrowserCdmCastUi::RemoveSession(
    const std::string& session_id,
    scoped_ptr<::media::SimpleCdmPromise> promise) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FORWARD_ON_CDM_THREAD(RemoveSession, session_id, base::Passed(&promise));
}

::media::CdmContext* BrowserCdmCastUi::GetCdmContext() {
  NOTREACHED();
  return nullptr;
}

}  // namespace media
}  // namespace chromecast
