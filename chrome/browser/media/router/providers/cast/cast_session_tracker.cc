// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/cast/cast_session_tracker.h"

#include "base/stl_util.h"
#include "chrome/browser/media/router/providers/cast/chrome_cast_message_handler.h"
#include "chrome/browser/media/router/providers/cast/dual_media_sink_service.h"
#include "components/cast_channel/cast_socket_service.h"

namespace media_router {

CastSessionTracker::Observer::~Observer() = default;

// static
CastSessionTracker* CastSessionTracker::GetInstance() {
  if (instance_for_test_)
    return instance_for_test_;

  static CastSessionTracker* instance = new CastSessionTracker(
      DualMediaSinkService::GetInstance()->GetCastMediaSinkServiceImpl(),
      GetCastMessageHandler(),
      cast_channel::CastSocketService::GetInstance()->task_runner());
  return instance;
}

void CastSessionTracker::AddObserver(CastSessionTracker::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void CastSessionTracker::RemoveObserver(
    CastSessionTracker::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

const CastSessionTracker::SessionMap& CastSessionTracker::sessions_by_sink_id()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return sessions_by_sink_id_;
}

CastSession* CastSessionTracker::GetSessionById(
    const std::string& session_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it =
      std::find_if(sessions_by_sink_id_.begin(), sessions_by_sink_id_.end(),
                   [&session_id](const auto& entry) {
                     return entry.second->session_id() == session_id;
                   });
  return it != sessions_by_sink_id_.end() ? it->second.get() : nullptr;
}

CastSessionTracker::CastSessionTracker(
    MediaSinkServiceBase* media_sink_service,
    cast_channel::CastMessageHandler* message_handler,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : media_sink_service_(media_sink_service),
      message_handler_(message_handler) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  // This is safe because |this| will never be destroyed (except in unit tests).
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&CastSessionTracker::InitOnIoThread,
                                       base::Unretained(this)));
}

CastSessionTracker::~CastSessionTracker() = default;

// This method needs to be separate from the constructor because the constructor
// needs to be called from the UI thread, but observers can only be added in an
// IO thread.
void CastSessionTracker::InitOnIoThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  media_sink_service_->AddObserver(this);
  message_handler_->AddObserver(this);
}

void CastSessionTracker::HandleReceiverStatusMessage(
    const MediaSinkInternal& sink,
    const base::Value& message) {
  const base::Value* status =
      message.FindKeyOfType("status", base::Value::Type::DICTIONARY);
  auto session = status ? CastSession::From(sink, *status) : nullptr;
  const MediaSink::Id& sink_id = sink.sink().id();
  if (!session) {
    if (sessions_by_sink_id_.erase(sink_id)) {
      for (auto& observer : observers_)
        observer.OnSessionRemoved(sink);
    }
    return;
  }

  auto it = sessions_by_sink_id_.find(sink_id);
  if (it == sessions_by_sink_id_.end()) {
    it = sessions_by_sink_id_.emplace(sink_id, std::move(session)).first;
  } else {
    it->second->UpdateSession(std::move(session));
  }

  for (auto& observer : observers_)
    observer.OnSessionAddedOrUpdated(sink, *it->second);
}

const MediaSinkInternal* CastSessionTracker::GetSinkByChannelId(
    int channel_id) const {
  for (const auto& sink : media_sink_service_->GetSinks()) {
    if (sink.second.cast_data().cast_channel_id == channel_id)
      return &sink.second;
  }
  return nullptr;
}

void CastSessionTracker::OnSinkAddedOrUpdated(const MediaSinkInternal& sink) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  message_handler_->RequestReceiverStatus(sink.cast_data().cast_channel_id);
}

void CastSessionTracker::OnSinkRemoved(const MediaSinkInternal& sink) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (sessions_by_sink_id_.erase(sink.sink().id())) {
    for (auto& observer : observers_)
      observer.OnSessionRemoved(sink);
  }
}

void CastSessionTracker::OnInternalMessage(
    int channel_id,
    const cast_channel::InternalMessage& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // It's possible for a session to have been started/discovered on a different
  // channel_id than the one we got the latest RECEIVER_STATUS on, but this
  // should be okay, since we are mapping everything back to the sink_id, which
  // should be constant.
  const MediaSinkInternal* sink = GetSinkByChannelId(channel_id);
  if (!sink) {
    DVLOG(2) << "Received message from channel without sink: " << channel_id;
    return;
  }

  if (message.type == cast_channel::CastMessageType::kReceiverStatus)
    HandleReceiverStatusMessage(*sink, message.message);
}

// static
void CastSessionTracker::SetInstanceForTest(
    CastSessionTracker* session_tracker) {
  instance_for_test_ = session_tracker;
}

void CastSessionTracker::SetSessionForTest(
    const MediaSink::Id& sink_id,
    std::unique_ptr<CastSession> session) {
  DCHECK(session);
  sessions_by_sink_id_[sink_id] = std::move(session);
}

// static
CastSessionTracker* CastSessionTracker::instance_for_test_ = nullptr;

}  // namespace media_router
