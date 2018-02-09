// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mock_filters.h"

#include "base/logging.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrictMock;

MATCHER(NotEmpty, "") {
  return !arg.empty();
}

namespace media {

MockPipelineClient::MockPipelineClient() = default;
MockPipelineClient::~MockPipelineClient() = default;

MockPipeline::MockPipeline() = default;
MockPipeline::~MockPipeline() = default;

void MockPipeline::Start(StartType start_type,
                         Demuxer* demuxer,
                         std::unique_ptr<Renderer> renderer,
                         Client* client,
                         const PipelineStatusCB& seek_cb) {
  Start(start_type, demuxer, &renderer, client, seek_cb);
}

void MockPipeline::Resume(std::unique_ptr<Renderer> renderer,
                          base::TimeDelta timestamp,
                          const PipelineStatusCB& seek_cb) {
  Resume(&renderer, timestamp, seek_cb);
}

MockDemuxer::MockDemuxer() = default;

MockDemuxer::~MockDemuxer() = default;

std::string MockDemuxer::GetDisplayName() const {
  return "MockDemuxer";
}

MockDemuxerStream::MockDemuxerStream(DemuxerStream::Type type)
    : type_(type), liveness_(LIVENESS_UNKNOWN) {
}

MockDemuxerStream::~MockDemuxerStream() = default;

DemuxerStream::Type MockDemuxerStream::type() const {
  return type_;
}

DemuxerStream::Liveness MockDemuxerStream::liveness() const {
  return liveness_;
}

AudioDecoderConfig MockDemuxerStream::audio_decoder_config() {
  DCHECK_EQ(type_, DemuxerStream::AUDIO);
  return audio_decoder_config_;
}

VideoDecoderConfig MockDemuxerStream::video_decoder_config() {
  DCHECK_EQ(type_, DemuxerStream::VIDEO);
  return video_decoder_config_;
}

void MockDemuxerStream::set_audio_decoder_config(
    const AudioDecoderConfig& config) {
  DCHECK_EQ(type_, DemuxerStream::AUDIO);
  audio_decoder_config_ = config;
}

void MockDemuxerStream::set_video_decoder_config(
    const VideoDecoderConfig& config) {
  DCHECK_EQ(type_, DemuxerStream::VIDEO);
  video_decoder_config_ = config;
}

void MockDemuxerStream::set_liveness(DemuxerStream::Liveness liveness) {
  liveness_ = liveness;
}

MockVideoDecoder::MockVideoDecoder(const std::string& decoder_name)
    : decoder_name_(decoder_name) {
  ON_CALL(*this, CanReadWithoutStalling()).WillByDefault(Return(true));
}

MockVideoDecoder::~MockVideoDecoder() = default;

std::string MockVideoDecoder::GetDisplayName() const {
  return decoder_name_;
}

MockAudioDecoder::MockAudioDecoder(const std::string& decoder_name)
    : decoder_name_(decoder_name) {}

MockAudioDecoder::~MockAudioDecoder() = default;

std::string MockAudioDecoder::GetDisplayName() const {
  return decoder_name_;
}

MockRendererClient::MockRendererClient() = default;

MockRendererClient::~MockRendererClient() = default;

MockVideoRenderer::MockVideoRenderer() = default;

MockVideoRenderer::~MockVideoRenderer() = default;

MockAudioRenderer::MockAudioRenderer() = default;

MockAudioRenderer::~MockAudioRenderer() = default;

MockRenderer::MockRenderer() = default;

MockRenderer::~MockRenderer() = default;

MockTimeSource::MockTimeSource() = default;

MockTimeSource::~MockTimeSource() = default;

MockTextTrack::MockTextTrack() = default;

MockTextTrack::~MockTextTrack() = default;

MockCdmClient::MockCdmClient() = default;

MockCdmClient::~MockCdmClient() = default;

MockDecryptor::MockDecryptor() = default;

MockDecryptor::~MockDecryptor() = default;

MockCdmContext::MockCdmContext() = default;

MockCdmContext::~MockCdmContext() = default;

int MockCdmContext::GetCdmId() const {
  return cdm_id_;
}

void MockCdmContext::set_cdm_id(int cdm_id) {
  cdm_id_ = cdm_id;
}

MockCdmPromise::MockCdmPromise(bool expect_success) {
  if (expect_success) {
    EXPECT_CALL(*this, resolve());
    EXPECT_CALL(*this, reject(_, _, _)).Times(0);
  } else {
    EXPECT_CALL(*this, resolve()).Times(0);
    EXPECT_CALL(*this, reject(_, _, NotEmpty()));
  }
}

MockCdmPromise::~MockCdmPromise() {
  // The EXPECT calls will verify that the promise is in fact fulfilled.
  MarkPromiseSettled();
}

MockCdmSessionPromise::MockCdmSessionPromise(bool expect_success,
                                             std::string* new_session_id) {
  if (expect_success) {
    EXPECT_CALL(*this, resolve(_)).WillOnce(SaveArg<0>(new_session_id));
    EXPECT_CALL(*this, reject(_, _, _)).Times(0);
  } else {
    EXPECT_CALL(*this, resolve(_)).Times(0);
    EXPECT_CALL(*this, reject(_, _, NotEmpty()));
  }
}

MockCdmSessionPromise::~MockCdmSessionPromise() {
  // The EXPECT calls will verify that the promise is in fact fulfilled.
  MarkPromiseSettled();
}

MockCdm::MockCdm(const std::string& key_system,
                 const url::Origin& security_origin,
                 const SessionMessageCB& session_message_cb,
                 const SessionClosedCB& session_closed_cb,
                 const SessionKeysChangeCB& session_keys_change_cb,
                 const SessionExpirationUpdateCB& session_expiration_update_cb)
    : key_system_(key_system),
      security_origin_(security_origin),
      session_message_cb_(session_message_cb),
      session_closed_cb_(session_closed_cb),
      session_keys_change_cb_(session_keys_change_cb),
      session_expiration_update_cb_(session_expiration_update_cb) {}

MockCdm::~MockCdm() = default;

void MockCdm::SetServerCertificate(const std::vector<uint8_t>& certificate,
                                   std::unique_ptr<SimpleCdmPromise> promise) {
  OnSetServerCertificate(certificate, promise);
}

void MockCdm::CreateSessionAndGenerateRequest(
    CdmSessionType session_type,
    EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data,
    std::unique_ptr<NewSessionCdmPromise> promise) {
  OnCreateSessionAndGenerateRequest(session_type, init_data_type, init_data,
                                    promise);
}

void MockCdm::LoadSession(CdmSessionType session_type,
                          const std::string& session_id,
                          std::unique_ptr<NewSessionCdmPromise> promise) {
  OnLoadSession(session_type, session_id, promise);
}

void MockCdm::UpdateSession(const std::string& session_id,
                            const std::vector<uint8_t>& response,
                            std::unique_ptr<SimpleCdmPromise> promise) {
  OnUpdateSession(session_id, response, promise);
}

void MockCdm::CloseSession(const std::string& session_id,
                           std::unique_ptr<SimpleCdmPromise> promise) {
  OnCloseSession(session_id, promise);
}

void MockCdm::RemoveSession(const std::string& session_id,
                            std::unique_ptr<SimpleCdmPromise> promise) {
  OnRemoveSession(session_id, promise);
}

void MockCdm::CallSessionMessageCB(const std::string& session_id,
                                   CdmMessageType message_type,
                                   const std::vector<uint8_t>& message) {
  session_message_cb_.Run(session_id, message_type, message);
}

void MockCdm::CallSessionClosedCB(const std::string& session_id) {
  session_closed_cb_.Run(session_id);
}

void MockCdm::CallSessionKeysChangeCB(const std::string& session_id,
                                      bool has_additional_usable_key,
                                      CdmKeysInfo keys_info) {
  session_keys_change_cb_.Run(session_id, has_additional_usable_key,
                              std::move(keys_info));
}

void MockCdm::CallSessionExpirationUpdateCB(const std::string& session_id,
                                            base::Time new_expiry_time) {
  session_expiration_update_cb_.Run(session_id, new_expiry_time);
}

MockCdmFactory::MockCdmFactory() = default;

MockCdmFactory::~MockCdmFactory() = default;

void MockCdmFactory::Create(
    const std::string& key_system,
    const url::Origin& security_origin,
    const CdmConfig& /* cdm_config */,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb,
    const CdmCreatedCB& cdm_created_cb) {
  // If no key system specified, notify that Create() failed.
  if (key_system.empty()) {
    cdm_created_cb.Run(nullptr, "CDM creation failed");
    return;
  }

  // Since there is a CDM, call |before_creation_cb_| first.
  if (!before_creation_cb_.is_null())
    before_creation_cb_.Run();

  // Create and return a new MockCdm. Keep a pointer to the created CDM so
  // that tests can access it. Calls to GetCdmContext() can be ignored.
  scoped_refptr<MockCdm> cdm = new StrictMock<MockCdm>(
      key_system, security_origin, session_message_cb, session_closed_cb,
      session_keys_change_cb, session_expiration_update_cb);
  created_cdm_ = cdm.get();
  EXPECT_CALL(*created_cdm_.get(), GetCdmContext());
  cdm_created_cb.Run(std::move(cdm), "");
}

MockCdm* MockCdmFactory::GetCreatedCdm() {
  return created_cdm_.get();
}

void MockCdmFactory::SetBeforeCreationCB(
    const base::Closure& before_creation_cb) {
  before_creation_cb_ = before_creation_cb;
}

MockStreamParser::MockStreamParser() = default;

MockStreamParser::~MockStreamParser() = default;

}  // namespace media
