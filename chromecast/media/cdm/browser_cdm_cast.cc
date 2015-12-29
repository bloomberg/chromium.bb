// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cdm/browser_cdm_cast.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "media/base/cdm_key_information.h"
#include "media/base/cdm_promise.h"
#include "media/cdm/player_tracker_impl.h"

namespace chromecast {
namespace media {

namespace {

// media::CdmPromiseTemplate implementation that wraps a promise so as to
// allow passing to other threads.
template <typename... T>
class CdmPromiseInternal : public ::media::CdmPromiseTemplate<T...> {
 public:
  CdmPromiseInternal(scoped_ptr<::media::CdmPromiseTemplate<T...>> promise)
      : task_runner_(base::ThreadTaskRunnerHandle::Get()),
        promise_(std::move(promise)) {}

  ~CdmPromiseInternal() final {
    // Promise must be resolved or rejected before destruction.
    DCHECK(!promise_);
  }

  // CdmPromiseTemplate<> implementation.
  void resolve(const T&... result) final;

  void reject(::media::MediaKeys::Exception exception,
              uint32_t system_code,
              const std::string& error_message) final {
    MarkPromiseSettled();
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&::media::CdmPromiseTemplate<T...>::reject,
                   base::Owned(promise_.release()),
                   exception, system_code, error_message));
  }

 private:
  using ::media::CdmPromiseTemplate<T...>::MarkPromiseSettled;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_ptr<::media::CdmPromiseTemplate<T...>> promise_;
};

template <typename... T>
void CdmPromiseInternal<T...>::resolve(const T&... result) {
  MarkPromiseSettled();
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&::media::CdmPromiseTemplate<T...>::resolve,
                 base::Owned(promise_.release()),
                 result...));
}

template <typename... T>
scoped_ptr<CdmPromiseInternal<T...>> BindPromiseToCurrentLoop(
    scoped_ptr<::media::CdmPromiseTemplate<T...>> promise) {
  return make_scoped_ptr(new CdmPromiseInternal<T...>(std::move(promise)));
}

}  // namespace

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

  player_tracker_impl_.reset(new ::media::PlayerTrackerImpl());

  session_message_cb_ = session_message_cb;
  session_closed_cb_ = session_closed_cb;
  legacy_session_error_cb_ = legacy_session_error_cb;
  session_keys_change_cb_ = session_keys_change_cb;
  session_expiration_update_cb_ = session_expiration_update_cb;

  InitializeInternal();
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

void BrowserCdmCast::OnSessionMessage(
    const std::string& session_id,
    const std::vector<uint8_t>& message,
    const GURL& destination_url,
    ::media::MediaKeys::MessageType message_type) {
  session_message_cb_.Run(session_id,
                          message_type,
                          message,
                          destination_url);
}

void BrowserCdmCast::OnSessionClosed(const std::string& session_id) {
  session_closed_cb_.Run(session_id);
}

void BrowserCdmCast::OnSessionKeysChange(const std::string& session_id,
                                         bool newly_usable_keys,
                                         ::media::CdmKeysInfo keys_info) {
  session_keys_change_cb_.Run(session_id, newly_usable_keys,
                              std::move(keys_info));

  if (newly_usable_keys)
    player_tracker_impl_->NotifyNewKey();
}

void BrowserCdmCast::KeyIdAndKeyPairsToInfo(
    const ::media::KeyIdAndKeyPairs& keys,
    ::media::CdmKeysInfo* keys_info) {
  DCHECK(keys_info);
  for (const std::pair<std::string, std::string>& key : keys) {
    scoped_ptr<::media::CdmKeyInformation> cdm_key_information(
        new ::media::CdmKeyInformation(key.first,
                                       ::media::CdmKeyInformation::USABLE, 0));
    keys_info->push_back(cdm_key_information.release());
  }
}

// A macro runs current member function on |task_runner_| thread.
#define FORWARD_ON_CDM_THREAD(param_fn, ...) \
  task_runner_->PostTask(                    \
      FROM_HERE,                             \
      base::Bind(&BrowserCdmCast::param_fn,  \
                 base::Unretained(browser_cdm_cast_.get()), ##__VA_ARGS__))

BrowserCdmCastUi::BrowserCdmCastUi(
    const scoped_refptr<BrowserCdmCast>& browser_cdm_cast,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : browser_cdm_cast_(browser_cdm_cast), task_runner_(task_runner) {}

BrowserCdmCastUi::~BrowserCdmCastUi() {
  DCHECK(thread_checker_.CalledOnValidThread());
  browser_cdm_cast_->AddRef();
  BrowserCdmCast* raw_cdm = browser_cdm_cast_.get();
  browser_cdm_cast_ = nullptr;
  task_runner_->ReleaseSoon(FROM_HERE, raw_cdm);
}

BrowserCdmCast* BrowserCdmCastUi::browser_cdm_cast() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return browser_cdm_cast_.get();
}

void BrowserCdmCastUi::SetServerCertificate(
    const std::vector<uint8_t>& certificate,
    scoped_ptr<::media::SimpleCdmPromise> promise) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FORWARD_ON_CDM_THREAD(
      SetServerCertificate, certificate,
      base::Passed(BindPromiseToCurrentLoop(std::move(promise))));
}

void BrowserCdmCastUi::CreateSessionAndGenerateRequest(
    ::media::MediaKeys::SessionType session_type,
    ::media::EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data,
    scoped_ptr<::media::NewSessionCdmPromise> promise) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FORWARD_ON_CDM_THREAD(
      CreateSessionAndGenerateRequest, session_type, init_data_type, init_data,
      base::Passed(BindPromiseToCurrentLoop(std::move(promise))));
}

void BrowserCdmCastUi::LoadSession(
    ::media::MediaKeys::SessionType session_type,
    const std::string& session_id,
    scoped_ptr<::media::NewSessionCdmPromise> promise) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FORWARD_ON_CDM_THREAD(
      LoadSession, session_type, session_id,
      base::Passed(BindPromiseToCurrentLoop(std::move(promise))));
}

void BrowserCdmCastUi::UpdateSession(
    const std::string& session_id,
    const std::vector<uint8_t>& response,
    scoped_ptr<::media::SimpleCdmPromise> promise) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FORWARD_ON_CDM_THREAD(
      UpdateSession, session_id, response,
      base::Passed(BindPromiseToCurrentLoop(std::move(promise))));
}

void BrowserCdmCastUi::CloseSession(
    const std::string& session_id,
    scoped_ptr<::media::SimpleCdmPromise> promise) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FORWARD_ON_CDM_THREAD(
      CloseSession, session_id,
      base::Passed(BindPromiseToCurrentLoop(std::move(promise))));
}

void BrowserCdmCastUi::RemoveSession(
    const std::string& session_id,
    scoped_ptr<::media::SimpleCdmPromise> promise) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FORWARD_ON_CDM_THREAD(
      RemoveSession, session_id,
      base::Passed(BindPromiseToCurrentLoop(std::move(promise))));
}

// A default empty implementation for subclasses that don't need to provide
// any key system specific initialization.
void BrowserCdmCast::InitializeInternal() {
}

}  // namespace media
}  // namespace chromecast
