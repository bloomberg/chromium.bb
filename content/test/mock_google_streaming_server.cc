// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_google_streaming_server.h"

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_byteorder.h"
#include "base/values.h"
#include "content/browser/speech/proto/google_streaming_api.pb.h"
#include "content/browser/speech/speech_recognition_engine.h"
#include "content/browser/speech/speech_recognition_manager_impl.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_status.h"

using base::HostToNet32;
using base::checked_cast;

namespace content {

MockGoogleStreamingServer::MockGoogleStreamingServer(Delegate* delegate)
    : delegate_(delegate),
      kDownstreamUrlFetcherId(
          SpeechRecognitionEngine::kDownstreamUrlFetcherIdForTesting),
      kUpstreamUrlFetcherId(
          SpeechRecognitionEngine::kUpstreamUrlFetcherIdForTesting) {
  url_fetcher_factory_.SetDelegateForTests(this);
}

MockGoogleStreamingServer::~MockGoogleStreamingServer() {
}

void MockGoogleStreamingServer::OnRequestStart(int fetcher_id) {
  if (fetcher_id != kDownstreamUrlFetcherId)
    return;

  // Extract request argument from the the request URI.
  std::string query = GetURLFetcher(true)->GetOriginalURL().query();
  const net::UnescapeRule::Type kUnescapeAll =
      net::UnescapeRule::NORMAL | net::UnescapeRule::SPACES |
      net::UnescapeRule::PATH_SEPARATORS |
      net::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS |
      net::UnescapeRule::REPLACE_PLUS_WITH_SPACE;
  for (const base::StringPiece& query_param :
       base::SplitStringPiece(query, "&", base::KEEP_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    std::vector<std::string> param_parts = base::SplitString(
        query_param, "=", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
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
  uint32_t prefix = HostToNet32(checked_cast<uint32_t>(msg_string.size()));
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

  fetcher->set_status(
      net::URLRequestStatus::FromError(success ? net::OK : net::ERR_FAILED));
  fetcher->set_response_code(success ? 200 : 500);
  fetcher->SetResponseString(http_response);
  fetcher->delegate()->OnURLFetchDownloadProgress(fetcher, 0, 0, 0);
}

// Can return NULL if the SpeechRecognizer has not requested the connection yet.
net::TestURLFetcher* MockGoogleStreamingServer::GetURLFetcher(
    bool downstream) const {
  return url_fetcher_factory_.GetFetcherByID(
      downstream ? kDownstreamUrlFetcherId : kUpstreamUrlFetcherId);
}

}  // namespace content
