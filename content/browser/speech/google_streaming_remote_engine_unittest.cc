// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <queue>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "content/browser/speech/audio_buffer.h"
#include "content/browser/speech/google_streaming_remote_engine.h"
#include "content/browser/speech/proto/google_streaming_api.pb.h"
#include "content/public/common/speech_recognition_error.h"
#include "content/public/common/speech_recognition_result.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::SpeechRecognitionHypothesis;
using content::SpeechRecognitionResult;
using net::URLRequestStatus;
using net::TestURLFetcher;
using net::TestURLFetcherFactory;

namespace speech {

// Note: the terms upstream and downstream are from the point-of-view of the
// client (engine_under_test_).

class GoogleStreamingRemoteEngineTest
    : public SpeechRecognitionEngineDelegate,
      public testing::Test {
 public:
  GoogleStreamingRemoteEngineTest()
      : last_number_of_upstream_chunks_seen_(0U),
        error_(content::SPEECH_RECOGNITION_ERROR_NONE) { }

  // Creates a speech recognition request and invokes its URL fetcher delegate
  // with the given test data.
  void CreateAndTestRequest(bool success, const std::string& http_response);

  // SpeechRecognitionRequestDelegate methods.
  virtual void OnSpeechRecognitionEngineResult(
      const SpeechRecognitionResult& result) OVERRIDE {
    results_.push(result);
  }
  virtual void OnSpeechRecognitionEngineError(
      const content::SpeechRecognitionError& error) OVERRIDE {
    error_ = error.code;
  }

  // testing::Test methods.
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

 protected:
  enum DownstreamError {
    DOWNSTREAM_ERROR_NONE,
    DOWNSTREAM_ERROR_HTTP500,
    DOWNSTREAM_ERROR_NETWORK,
    DOWNSTREAM_ERROR_WEBSERVICE_NO_MATCH
  };
  static bool ResultsAreEqual(const SpeechRecognitionResult& a,
                              const SpeechRecognitionResult& b);
  static std::string SerializeProtobufResponse(
      const proto::SpeechRecognitionEvent& msg);
  static std::string ToBigEndian32(uint32 value);

  TestURLFetcher* GetUpstreamFetcher();
  TestURLFetcher* GetDownstreamFetcher();
  void StartMockRecognition();
  void EndMockRecognition();
  void InjectDummyAudioChunk();
  size_t UpstreamChunksUploadedFromLastCall();
  void ProvideMockResultDownstream(const SpeechRecognitionResult& result);
  void ExpectResultReceived(const SpeechRecognitionResult& result);
  void CloseMockDownstream(DownstreamError error);

  scoped_ptr<GoogleStreamingRemoteEngine> engine_under_test_;
  TestURLFetcherFactory url_fetcher_factory_;
  size_t last_number_of_upstream_chunks_seen_;
  MessageLoop message_loop_;
  std::string response_buffer_;
  content::SpeechRecognitionErrorCode error_;
  std::queue<SpeechRecognitionResult> results_;
};

TEST_F(GoogleStreamingRemoteEngineTest, SingleDefinitiveResult) {
  StartMockRecognition();
  ASSERT_TRUE(GetUpstreamFetcher());
  ASSERT_EQ(0U, UpstreamChunksUploadedFromLastCall());

  // Inject some dummy audio chunks and check a corresponding chunked upload
  // is performed every time on the server.
  for (int i = 0; i < 3; ++i) {
    InjectDummyAudioChunk();
    ASSERT_EQ(1U, UpstreamChunksUploadedFromLastCall());
  }

  // Ensure that a final (empty) audio chunk is uploaded on chunks end.
  engine_under_test_->AudioChunksEnded();
  ASSERT_EQ(1U, UpstreamChunksUploadedFromLastCall());
  ASSERT_TRUE(engine_under_test_->IsRecognitionPending());

  // Simulate a protobuf message streamed from the server containing a single
  // result with two hypotheses.
  SpeechRecognitionResult result;
  result.is_provisional = false;
  result.hypotheses.push_back(
      SpeechRecognitionHypothesis(UTF8ToUTF16("hypothesis 1"), 0.1F));
  result.hypotheses.push_back(
      SpeechRecognitionHypothesis(UTF8ToUTF16("hypothesis 2"), 0.2F));

  ProvideMockResultDownstream(result);
  ExpectResultReceived(result);
  ASSERT_TRUE(engine_under_test_->IsRecognitionPending());

  // Ensure everything is closed cleanly after the downstream is closed.
  CloseMockDownstream(DOWNSTREAM_ERROR_NONE);
  ASSERT_FALSE(engine_under_test_->IsRecognitionPending());
  EndMockRecognition();
  ASSERT_EQ(content::SPEECH_RECOGNITION_ERROR_NONE, error_);
  ASSERT_EQ(0U, results_.size());
}

TEST_F(GoogleStreamingRemoteEngineTest, SeveralStreamingResults) {
  StartMockRecognition();
  ASSERT_TRUE(GetUpstreamFetcher());
  ASSERT_EQ(0U, UpstreamChunksUploadedFromLastCall());

  for (int i = 0; i < 4; ++i) {
    InjectDummyAudioChunk();
    ASSERT_EQ(1U, UpstreamChunksUploadedFromLastCall());

    SpeechRecognitionResult result;
    result.is_provisional = (i % 2 == 0);  // Alternate result types.
    float confidence = result.is_provisional ? 0.0F : (i * 0.1F);
    result.hypotheses.push_back(
        SpeechRecognitionHypothesis(UTF8ToUTF16("hypothesis"), confidence));

    ProvideMockResultDownstream(result);
    ExpectResultReceived(result);
    ASSERT_TRUE(engine_under_test_->IsRecognitionPending());
  }

  // Ensure that a final (empty) audio chunk is uploaded on chunks end.
  engine_under_test_->AudioChunksEnded();
  ASSERT_EQ(1U, UpstreamChunksUploadedFromLastCall());
  ASSERT_TRUE(engine_under_test_->IsRecognitionPending());

  // Simulate a final definitive result.
  SpeechRecognitionResult result;
  result.is_provisional = false;
  result.hypotheses.push_back(
      SpeechRecognitionHypothesis(UTF8ToUTF16("The final result"), 1.0F));
  ProvideMockResultDownstream(result);
  ExpectResultReceived(result);
  ASSERT_TRUE(engine_under_test_->IsRecognitionPending());

  // Ensure everything is closed cleanly after the downstream is closed.
  CloseMockDownstream(DOWNSTREAM_ERROR_NONE);
  ASSERT_FALSE(engine_under_test_->IsRecognitionPending());
  EndMockRecognition();
  ASSERT_EQ(content::SPEECH_RECOGNITION_ERROR_NONE, error_);
  ASSERT_EQ(0U, results_.size());
}

TEST_F(GoogleStreamingRemoteEngineTest, NoFinalResultAfterAudioChunksEnded) {
  StartMockRecognition();
  ASSERT_TRUE(GetUpstreamFetcher());
  ASSERT_EQ(0U, UpstreamChunksUploadedFromLastCall());

  // Simulate one pushed audio chunk.
  InjectDummyAudioChunk();
  ASSERT_EQ(1U, UpstreamChunksUploadedFromLastCall());

  // Simulate the corresponding definitive result.
  SpeechRecognitionResult result;
  result.hypotheses.push_back(
      SpeechRecognitionHypothesis(UTF8ToUTF16("hypothesis"), 1.0F));
  ProvideMockResultDownstream(result);
  ExpectResultReceived(result);
  ASSERT_TRUE(engine_under_test_->IsRecognitionPending());

  // Simulate a silent downstream closure after |AudioChunksEnded|.
  engine_under_test_->AudioChunksEnded();
  ASSERT_EQ(1U, UpstreamChunksUploadedFromLastCall());
  ASSERT_TRUE(engine_under_test_->IsRecognitionPending());
  CloseMockDownstream(DOWNSTREAM_ERROR_NONE);

  // Expect an empty result, aimed at notifying recognition ended with no
  // actual results nor errors.
  SpeechRecognitionResult empty_result;
  ExpectResultReceived(empty_result);

  // Ensure everything is closed cleanly after the downstream is closed.
  ASSERT_FALSE(engine_under_test_->IsRecognitionPending());
  EndMockRecognition();
  ASSERT_EQ(content::SPEECH_RECOGNITION_ERROR_NONE, error_);
  ASSERT_EQ(0U, results_.size());
}

TEST_F(GoogleStreamingRemoteEngineTest, NoMatchError) {
  StartMockRecognition();
  ASSERT_TRUE(GetUpstreamFetcher());
  ASSERT_EQ(0U, UpstreamChunksUploadedFromLastCall());

  for (int i = 0; i < 3; ++i)
    InjectDummyAudioChunk();
  engine_under_test_->AudioChunksEnded();
  ASSERT_EQ(4U, UpstreamChunksUploadedFromLastCall());
  ASSERT_TRUE(engine_under_test_->IsRecognitionPending());

  // Simulate only a provisional result.
  SpeechRecognitionResult result;
  result.is_provisional = true;
  result.hypotheses.push_back(
      SpeechRecognitionHypothesis(UTF8ToUTF16("The final result"), 0.0F));
  ProvideMockResultDownstream(result);
  ExpectResultReceived(result);
  ASSERT_TRUE(engine_under_test_->IsRecognitionPending());

  CloseMockDownstream(DOWNSTREAM_ERROR_WEBSERVICE_NO_MATCH);

  // Expect an empty result.
  ASSERT_FALSE(engine_under_test_->IsRecognitionPending());
  EndMockRecognition();
  SpeechRecognitionResult empty_result;
  ExpectResultReceived(empty_result);
}

TEST_F(GoogleStreamingRemoteEngineTest, HTTPError) {
  StartMockRecognition();
  ASSERT_TRUE(GetUpstreamFetcher());
  ASSERT_EQ(0U, UpstreamChunksUploadedFromLastCall());

  InjectDummyAudioChunk();
  ASSERT_EQ(1U, UpstreamChunksUploadedFromLastCall());

  // Close the downstream with a HTTP 500 error.
  CloseMockDownstream(DOWNSTREAM_ERROR_HTTP500);

  // Expect a SPEECH_RECOGNITION_ERROR_NETWORK error to be raised.
  ASSERT_FALSE(engine_under_test_->IsRecognitionPending());
  EndMockRecognition();
  ASSERT_EQ(content::SPEECH_RECOGNITION_ERROR_NETWORK, error_);
  ASSERT_EQ(0U, results_.size());
}

TEST_F(GoogleStreamingRemoteEngineTest, NetworkError) {
  StartMockRecognition();
  ASSERT_TRUE(GetUpstreamFetcher());
  ASSERT_EQ(0U, UpstreamChunksUploadedFromLastCall());

  InjectDummyAudioChunk();
  ASSERT_EQ(1U, UpstreamChunksUploadedFromLastCall());

  // Close the downstream fetcher simulating a network failure.
  CloseMockDownstream(DOWNSTREAM_ERROR_NETWORK);

  // Expect a SPEECH_RECOGNITION_ERROR_NETWORK error to be raised.
  ASSERT_FALSE(engine_under_test_->IsRecognitionPending());
  EndMockRecognition();
  ASSERT_EQ(content::SPEECH_RECOGNITION_ERROR_NETWORK, error_);
  ASSERT_EQ(0U, results_.size());
}

void GoogleStreamingRemoteEngineTest::SetUp() {
  engine_under_test_.reset(
      new  GoogleStreamingRemoteEngine(NULL /*URLRequestContextGetter*/));
  engine_under_test_->set_delegate(this);
}

void GoogleStreamingRemoteEngineTest::TearDown() {
  engine_under_test_.reset();
}

TestURLFetcher* GoogleStreamingRemoteEngineTest::GetUpstreamFetcher() {
  return url_fetcher_factory_.GetFetcherByID(
        GoogleStreamingRemoteEngine::kUpstreamUrlFetcherIdForTests);
}

TestURLFetcher* GoogleStreamingRemoteEngineTest::GetDownstreamFetcher() {
  return url_fetcher_factory_.GetFetcherByID(
        GoogleStreamingRemoteEngine::kDownstreamUrlFetcherIdForTests);
}

// Starts recognition on the engine, ensuring that both stream fetchers are
// created.
void GoogleStreamingRemoteEngineTest::StartMockRecognition() {
  DCHECK(engine_under_test_.get());

  ASSERT_FALSE(engine_under_test_->IsRecognitionPending());

  engine_under_test_->StartRecognition();
  ASSERT_TRUE(engine_under_test_->IsRecognitionPending());

  TestURLFetcher* upstream_fetcher = GetUpstreamFetcher();
  ASSERT_TRUE(upstream_fetcher);
  upstream_fetcher->set_url(upstream_fetcher->GetOriginalURL());

  TestURLFetcher* downstream_fetcher = GetDownstreamFetcher();
  ASSERT_TRUE(downstream_fetcher);
  downstream_fetcher->set_url(downstream_fetcher->GetOriginalURL());
}

void GoogleStreamingRemoteEngineTest::EndMockRecognition() {
  DCHECK(engine_under_test_.get());
  engine_under_test_->EndRecognition();
  ASSERT_FALSE(engine_under_test_->IsRecognitionPending());

  // TODO(primiano): In order to be very pedantic we should check that both the
  // upstream and downstream URL fetchers have been disposed at this time.
  // Unfortunately it seems that there is no direct way to detect (in tests)
  // if a url_fetcher has been freed or not, since they are not automatically
  // de-registered from the TestURLFetcherFactory on destruction.
}

void GoogleStreamingRemoteEngineTest::InjectDummyAudioChunk() {
  unsigned char dummy_audio_buffer_data[2] = {'\0', '\0'};
  scoped_refptr<AudioChunk> dummy_audio_chunk(
      new AudioChunk(&dummy_audio_buffer_data[0],
                     sizeof(dummy_audio_buffer_data),
                     2 /* bytes per sample */));
  DCHECK(engine_under_test_.get());
  engine_under_test_->TakeAudioChunk(*dummy_audio_chunk);
}

size_t GoogleStreamingRemoteEngineTest::UpstreamChunksUploadedFromLastCall() {
  TestURLFetcher* upstream_fetcher = GetUpstreamFetcher();
  DCHECK(upstream_fetcher);
  const size_t number_of_chunks = upstream_fetcher->upload_chunks().size();
  DCHECK_GE(number_of_chunks, last_number_of_upstream_chunks_seen_);
  const size_t new_chunks = number_of_chunks -
                            last_number_of_upstream_chunks_seen_;
  last_number_of_upstream_chunks_seen_ = number_of_chunks;
  return new_chunks;
}

void GoogleStreamingRemoteEngineTest::ProvideMockResultDownstream(
    const SpeechRecognitionResult& result) {
  TestURLFetcher* downstream_fetcher = GetDownstreamFetcher();

  ASSERT_TRUE(downstream_fetcher);
  downstream_fetcher->set_status(URLRequestStatus(/* default=SUCCESS */));
  downstream_fetcher->set_response_code(200);

  proto::SpeechRecognitionEvent proto_event;
  proto_event.set_status(proto::SpeechRecognitionEvent::STATUS_SUCCESS);
  proto::SpeechRecognitionResult* proto_result = proto_event.add_result();
  proto_result->set_final(!result.is_provisional);
  for (size_t i = 0; i < result.hypotheses.size(); ++i) {
    proto::SpeechRecognitionAlternative* proto_alternative =
        proto_result->add_alternative();
    const SpeechRecognitionHypothesis& hypothesis = result.hypotheses[i];
    proto_alternative->set_confidence(hypothesis.confidence);
    proto_alternative->set_transcript(UTF16ToUTF8(hypothesis.utterance));
  }

  std::string response_string = SerializeProtobufResponse(proto_event);
  response_buffer_.append(response_string);
  downstream_fetcher->SetResponseString(response_buffer_);
  downstream_fetcher->delegate()->OnURLFetchDownloadProgress(
      downstream_fetcher,
      response_buffer_.size(),
      -1 /* total response length not used */);
}

void GoogleStreamingRemoteEngineTest::CloseMockDownstream(
    DownstreamError error) {
  TestURLFetcher* downstream_fetcher = GetDownstreamFetcher();
  ASSERT_TRUE(downstream_fetcher);

  const URLRequestStatus::Status fetcher_status =
      (error == DOWNSTREAM_ERROR_NETWORK) ? URLRequestStatus::FAILED :
                                            URLRequestStatus::SUCCESS;
  downstream_fetcher->set_status(URLRequestStatus(fetcher_status, 0));
  downstream_fetcher->set_response_code(
      (error == DOWNSTREAM_ERROR_HTTP500) ? 500 : 200);

  if (error == DOWNSTREAM_ERROR_WEBSERVICE_NO_MATCH) {
    // Send empty response.
    proto::SpeechRecognitionEvent response;
    response_buffer_.append(SerializeProtobufResponse(response));
  }
  downstream_fetcher->SetResponseString(response_buffer_);
  downstream_fetcher->delegate()->OnURLFetchComplete(downstream_fetcher);
}

void GoogleStreamingRemoteEngineTest::ExpectResultReceived(
    const SpeechRecognitionResult& result) {
  ASSERT_GE(1U, results_.size());
  ASSERT_TRUE(ResultsAreEqual(result, results_.front()));
  results_.pop();
}

bool GoogleStreamingRemoteEngineTest::ResultsAreEqual(
    const SpeechRecognitionResult& a, const SpeechRecognitionResult& b) {
  if (a.is_provisional != b.is_provisional ||
      a.hypotheses.size() != b.hypotheses.size()) {
    return false;
  }
  for (size_t i = 0; i < a.hypotheses.size(); ++i) {
    const SpeechRecognitionHypothesis& hyp_a = a.hypotheses[i];
    const SpeechRecognitionHypothesis& hyp_b = b.hypotheses[i];
    if (hyp_a.utterance != hyp_b.utterance ||
        hyp_a.confidence != hyp_b.confidence) {
      return false;
    }
  }
  return true;
}

std::string GoogleStreamingRemoteEngineTest::SerializeProtobufResponse(
    const proto::SpeechRecognitionEvent& msg) {
  std::string msg_string;
  msg.SerializeToString(&msg_string);

  // Prepend 4 byte prefix length indication to the protobuf message as
  // envisaged by the google streaming recognition webservice protocol.
  msg_string.insert(0, ToBigEndian32(msg_string.size()));
  return msg_string;
}

std::string GoogleStreamingRemoteEngineTest::ToBigEndian32(uint32 value) {
  char raw_data[4];
  raw_data[0] = static_cast<uint8>((value >> 24) & 0xFF);
  raw_data[1] = static_cast<uint8>((value >> 16) & 0xFF);
  raw_data[2] = static_cast<uint8>((value >> 8) & 0xFF);
  raw_data[3] = static_cast<uint8>(value & 0xFF);
  return std::string(raw_data, sizeof(raw_data));
}

}  // namespace speech
