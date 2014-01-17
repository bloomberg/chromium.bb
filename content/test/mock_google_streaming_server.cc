// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_google_streaming_server.h"

#include "base/bind.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_byteorder.h"
#include "base/values.h"
#include "content/browser/speech/google_streaming_remote_engine.h"
#include "content/browser/speech/proto/google_streaming_api.pb.h"
#include "content/browser/speech/speech_recognition_manager_impl.h"
#include "net/base/escape.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_status.h"

using base::HostToNet32;
using base::checked_cast;

namespace content {

MockGoogleStreamingServer::MockGoogleStreamingServer(Delegate* delegate)
    : delegate_(delegate),
      kDownstreamUrlFetcherId(
          GoogleStreamingRemoteEngine::kDownstreamUrlFetcherIdForTesting),
      kUpstreamUrlFetcherId(
          GoogleStreamingRemoteEngine::kUpstreamUrlFetcherIdForTesting) {
  url_fetcher_factory_.SetDelegateForTests(this);
}

MockGoogleStreamingServer::~MockGoogleStreamingServer() {
}

void MockGoogleStreamingServer::OnRequestStart(int fetcher_id) {
  if (fetcher_id != kDownstreamUrlFetcherId)
    return;

  // Extract request argument from the the request URI.
  std::string query = GetURLFetcher(true)->GetOriginalURL().query();
  std::vector<std::string> query_params;
  Tokenize(query, "&", &query_params);
  const net::UnescapeRule::Type kUnescapeAll =
      net::UnescapeRule::NORMAL |
      net::UnescapeRule::SPACES |
      net::UnescapeRule::URL_SPECIAL_CHARS |
      net::UnescapeRule::REPLACE_PLUS_WITH_SPACE;
  for (size_t i = 0; i < query_params.size(); ++i) {
    const std::string query_param = query_params[i];
    std::vector<std::string> param_parts;
    Tokenize(query_param, "=", &param_parts);
    if (param_parts.size() != 2)
      continue;
    std::string param_key = net::UnescapeURLComponent(param_parts[0],
                                                      kUnescapeAll);
    std::string param_value = net::UnescapeURLComponent(param_parts[1],
                                                        kUnescapeAll);
    if (param_key == "lang") {
      request_language = param_value;
    } else if (param_key == "lm") {
      request_grammar = param_value;
    }
  }

  delegate_->OnClientConnected();
}

void MockGoogleStreamingServer::OnChunkUpload(int fetcher_id) {
  if (fetcher_id != kUpstreamUrlFetcherId)
      return;
  delegate_->OnClientAudioUpload();
  if (GetURLFetcher(false)->did_receive_last_chunk())
    delegate_->OnClientAudioUploadComplete();
}

void MockGoogleStreamingServer::OnRequestEnd(int fetcher_id) {
  if (fetcher_id != kDownstreamUrlFetcherId)
    return;
  url_fetcher_factory_.RemoveFetcherFromMap(kDownstreamUrlFetcherId);
  delegate_->OnClientDisconnected();
}

void MockGoogleStreamingServer::SimulateResult(
    const SpeechRecognitionResult& result) {
  proto::SpeechRecognitionEvent proto_event;
  proto_event.set_status(proto::SpeechRecognitionEvent::STATUS_SUCCESS);
  proto::SpeechRecognitionResult* proto_result = proto_event.add_result();
  proto_result->set_final(!result.is_provisional);
  for (size_t i = 0; i < result.hypotheses.size(); ++i) {
    proto::SpeechRecognitionAlternative* proto_alternative =
        proto_result->add_alternative();
    const SpeechRecognitionHypothesis& hypothesis = result.hypotheses[i];
    proto_alternative->set_confidence(hypothesis.confidence);
    proto_alternative->set_transcript(base::UTF16ToUTF8(hypothesis.utterance));
  }

  std::string msg_string;
  proto_event.SerializeToString(&msg_string);

  // Prepend 4 byte prefix length indication to the protobuf message as
  // envisaged by the google streaming recognition webservice protocol.
  uint32 prefix = HostToNet32(checked_cast<uint32>(msg_string.size()));
  msg_string.insert(0, reinterpret_cast<char*>(&prefix), sizeof(prefix));

  SimulateServerResponse(true, msg_string);
}

void MockGoogleStreamingServer::SimulateServerFailure() {
  SimulateServerResponse(false, "");
}

void MockGoogleStreamingServer::SimulateMalformedResponse() {
  std::string json =
      "{\"status\":0,\"hypotheses\":""[{\"unknownkey\":\"hello\"}]}";
  SimulateServerResponse(true, json);
}

const std::string& MockGoogleStreamingServer::GetRequestLanguage() const {
  return request_language;
}

const std::string& MockGoogleStreamingServer::GetRequestGrammar() const {
  return request_grammar;
}

void MockGoogleStreamingServer::SimulateServerResponse(
    bool success, const std::string& http_response) {
  net::TestURLFetcher* fetcher = GetURLFetcher(true);

  net::URLRequestStatus status;
  status.set_status(success ? net::URLRequestStatus::SUCCESS :
                              net::URLRequestStatus::FAILED);
  fetcher->set_status(status);
  fetcher->set_response_code(success ? 200 : 500);
  fetcher->SetResponseString(http_response);
  fetcher->delegate()->OnURLFetchDownloadProgress(fetcher, 0, 0);
}

// Can return NULL if the SpeechRecognizer has not requested the connection yet.
net::TestURLFetcher* MockGoogleStreamingServer::GetURLFetcher(
    bool downstream) const {
  return url_fetcher_factory_.GetFetcherByID(
      downstream ? kDownstreamUrlFetcherId : kUpstreamUrlFetcherId);
}

}  // namespace content
