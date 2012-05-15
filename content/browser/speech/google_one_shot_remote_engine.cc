// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/google_one_shot_remote_engine.h"

#include <vector>

#include "base/json/json_reader.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/values.h"
#include "content/browser/speech/audio_buffer.h"
#include "content/common/net/url_fetcher_impl.h"
#include "content/public/common/speech_recognition_error.h"
#include "content/public/common/speech_recognition_result.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

using content::SpeechRecognitionError;
using content::SpeechRecognitionHypothesis;
using content::SpeechRecognitionResult;

namespace {

const char* const kDefaultSpeechRecognitionUrl =
    "https://www.google.com/speech-api/v1/recognize?xjerr=1&client=chromium&";
const char* const kStatusString = "status";
const char* const kHypothesesString = "hypotheses";
const char* const kUtteranceString = "utterance";
const char* const kConfidenceString = "confidence";
const int kWebServiceStatusNoError = 0;
const int kWebServiceStatusNoSpeech = 4;
const int kWebServiceStatusNoMatch = 5;
const speech::AudioEncoder::Codec kDefaultAudioCodec =
    speech::AudioEncoder::CODEC_FLAC;
// TODO(satish): Remove this hardcoded value once the page is allowed to
// set this via an attribute.
const int kMaxResults = 6;

bool ParseServerResponse(const std::string& response_body,
                         SpeechRecognitionResult* result,
                         SpeechRecognitionError* error) {
  if (response_body.empty()) {
    LOG(WARNING) << "ParseServerResponse: Response was empty.";
    return false;
  }
  DVLOG(1) << "ParseServerResponse: Parsing response " << response_body;

  // Parse the response, ignoring comments.
  std::string error_msg;
  scoped_ptr<Value> response_value(base::JSONReader::ReadAndReturnError(
      response_body, base::JSON_PARSE_RFC, NULL, &error_msg));
  if (response_value == NULL) {
    LOG(WARNING) << "ParseServerResponse: JSONReader failed : " << error_msg;
    return false;
  }

  if (!response_value->IsType(Value::TYPE_DICTIONARY)) {
    VLOG(1) << "ParseServerResponse: Unexpected response type "
            << response_value->GetType();
    return false;
  }
  const DictionaryValue* response_object =
      static_cast<DictionaryValue*>(response_value.get());

  // Get the status.
  int status;
  if (!response_object->GetInteger(kStatusString, &status)) {
    VLOG(1) << "ParseServerResponse: " << kStatusString
            << " is not a valid integer value.";
    return false;
  }

  // Process the status.
  switch (status) {
    case kWebServiceStatusNoError:
      break;
    case kWebServiceStatusNoSpeech:
      error->code = content::SPEECH_RECOGNITION_ERROR_NO_SPEECH;
      return false;
    case kWebServiceStatusNoMatch:
      error->code = content::SPEECH_RECOGNITION_ERROR_NO_MATCH;
      return false;
    default:
      error->code = content::SPEECH_RECOGNITION_ERROR_NETWORK;
      // Other status codes should not be returned by the server.
      VLOG(1) << "ParseServerResponse: unexpected status code " << status;
      return false;
  }

  // Get the hypotheses.
  Value* hypotheses_value = NULL;
  if (!response_object->Get(kHypothesesString, &hypotheses_value)) {
    VLOG(1) << "ParseServerResponse: Missing hypotheses attribute.";
    return false;
  }

  DCHECK(hypotheses_value);
  if (!hypotheses_value->IsType(Value::TYPE_LIST)) {
    VLOG(1) << "ParseServerResponse: Unexpected hypotheses type "
            << hypotheses_value->GetType();
    return false;
  }

  const ListValue* hypotheses_list = static_cast<ListValue*>(hypotheses_value);

  // For now we support only single shot recognition, so we are giving only a
  // final result, consisting of one fragment (with one or more hypotheses).
  size_t index = 0;
  for (; index < hypotheses_list->GetSize(); ++index) {
    Value* hypothesis = NULL;
    if (!hypotheses_list->Get(index, &hypothesis)) {
      LOG(WARNING) << "ParseServerResponse: Unable to read hypothesis value.";
      break;
    }
    DCHECK(hypothesis);
    if (!hypothesis->IsType(Value::TYPE_DICTIONARY)) {
      LOG(WARNING) << "ParseServerResponse: Unexpected value type "
                   << hypothesis->GetType();
      break;
    }

    const DictionaryValue* hypothesis_value =
        static_cast<DictionaryValue*>(hypothesis);
    string16 utterance;

    if (!hypothesis_value->GetString(kUtteranceString, &utterance)) {
      LOG(WARNING) << "ParseServerResponse: Missing utterance value.";
      break;
    }

    // It is not an error if the 'confidence' field is missing.
    double confidence = 0.0;
    hypothesis_value->GetDouble(kConfidenceString, &confidence);
    result->hypotheses.push_back(SpeechRecognitionHypothesis(utterance,
                                                             confidence));
  }

  if (index < hypotheses_list->GetSize()) {
    result->hypotheses.clear();
    return false;
  }
  return true;
}

}  // namespace

namespace speech {

const int GoogleOneShotRemoteEngine::kAudioPacketIntervalMs = 100;
int GoogleOneShotRemoteEngine::url_fetcher_id_for_tests = 0;

GoogleOneShotRemoteEngine::GoogleOneShotRemoteEngine(
    net::URLRequestContextGetter* context)
    : url_context_(context) {
}

GoogleOneShotRemoteEngine::~GoogleOneShotRemoteEngine() {}

void GoogleOneShotRemoteEngine::SetConfig(
    const SpeechRecognitionEngineConfig& config) {
  config_ = config;
}

void GoogleOneShotRemoteEngine::StartRecognition() {
  DCHECK(delegate());
  DCHECK(!url_fetcher_.get());
  std::string lang_param = config_.language;

  if (lang_param.empty() && url_context_) {
    // If no language is provided then we use the first from the accepted
    // language list. If this list is empty then it defaults to "en-US".
    // Example of the contents of this list: "es,en-GB;q=0.8", ""
    net::URLRequestContext* request_context =
        url_context_->GetURLRequestContext();
    DCHECK(request_context);
    std::string accepted_language_list = request_context->accept_language();
    size_t separator = accepted_language_list.find_first_of(",;");
    lang_param = accepted_language_list.substr(0, separator);
  }

  if (lang_param.empty())
    lang_param = "en-US";

  std::vector<std::string> parts;
  parts.push_back("lang=" + net::EscapeQueryParamValue(lang_param, true));

  if (!config_.grammars.empty()) {
    DCHECK_EQ(config_.grammars.size(), 1U);
    parts.push_back("lm=" + net::EscapeQueryParamValue(config_.grammars[0].url,
                                                       true));
  }

  if (!config_.hardware_info.empty())
    parts.push_back("xhw=" + net::EscapeQueryParamValue(config_.hardware_info,
                                                        true));
  parts.push_back("maxresults=" + base::IntToString(kMaxResults));
  parts.push_back(config_.filter_profanities ? "pfilter=2" : "pfilter=0");

  GURL url(std::string(kDefaultSpeechRecognitionUrl) + JoinString(parts, '&'));

  encoder_.reset(AudioEncoder::Create(kDefaultAudioCodec,
                                      config_.audio_sample_rate,
                                      config_.audio_num_bits_per_sample));
  DCHECK(encoder_.get());
  url_fetcher_.reset(URLFetcherImpl::Create(url_fetcher_id_for_tests,
                                            url,
                                            URLFetcherImpl::POST,
                                            this));
  url_fetcher_->SetChunkedUpload(encoder_->mime_type());
  url_fetcher_->SetRequestContext(url_context_);
  url_fetcher_->SetReferrer(config_.origin_url);

  // The speech recognition API does not require user identification as part
  // of requests, so we don't send cookies or auth data for these requests to
  // prevent any accidental connection between users who are logged into the
  // domain for other services (e.g. bookmark sync) with the speech requests.
  url_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES |
                             net::LOAD_DO_NOT_SEND_COOKIES |
                             net::LOAD_DO_NOT_SEND_AUTH_DATA);
  url_fetcher_->Start();
}

void GoogleOneShotRemoteEngine::EndRecognition() {
  url_fetcher_.reset();
}

void GoogleOneShotRemoteEngine::TakeAudioChunk(const AudioChunk& data) {
  DCHECK(url_fetcher_.get());
  DCHECK(encoder_.get());
  DCHECK_EQ(data.bytes_per_sample(), config_.audio_num_bits_per_sample / 8);
  encoder_->Encode(data);
  scoped_refptr<AudioChunk> encoded_data(encoder_->GetEncodedDataAndClear());
  url_fetcher_->AppendChunkToUpload(encoded_data->AsString(), false);
}

void GoogleOneShotRemoteEngine::AudioChunksEnded() {
  DCHECK(url_fetcher_.get());
  DCHECK(encoder_.get());

  // UploadAudioChunk requires a non-empty final buffer. So we encode a packet
  // of silence in case encoder had no data already.
  std::vector<int16> samples(
      config_.audio_sample_rate * kAudioPacketIntervalMs / 1000);
  scoped_refptr<AudioChunk> dummy_chunk(
      new AudioChunk(reinterpret_cast<uint8*>(&samples[0]),
                     samples.size() * sizeof(int16),
                     encoder_->bits_per_sample() / 8));
  encoder_->Encode(*dummy_chunk);
  encoder_->Flush();
  scoped_refptr<AudioChunk> encoded_dummy_data(
      encoder_->GetEncodedDataAndClear());
  DCHECK(!encoded_dummy_data->IsEmpty());
  encoder_.reset();

  url_fetcher_->AppendChunkToUpload(encoded_dummy_data->AsString(), true);
}

void GoogleOneShotRemoteEngine::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK_EQ(url_fetcher_.get(), source);
  SpeechRecognitionResult result;
  SpeechRecognitionError error(content::SPEECH_RECOGNITION_ERROR_NETWORK);
  std::string data;

  // The default error code in case of parse errors is NETWORK_FAILURE, however
  // ParseServerResponse can change the error to a more appropriate one.
  bool error_occurred = (!source->GetStatus().is_success() ||
                        source->GetResponseCode() != 200 ||
                        !source->GetResponseAsString(&data) ||
                        !ParseServerResponse(data, &result, &error));
  url_fetcher_.reset();
  if (error_occurred) {
    DVLOG(1) << "GoogleOneShotRemoteEngine: Network Error " << error.code;
    delegate()->OnSpeechRecognitionEngineError(error);
  } else {
    DVLOG(1) << "GoogleOneShotRemoteEngine: Invoking delegate with result.";
    delegate()->OnSpeechRecognitionEngineResult(result);
  }
}

bool GoogleOneShotRemoteEngine::IsRecognitionPending() const {
  return url_fetcher_ != NULL;
}

int GoogleOneShotRemoteEngine::GetDesiredAudioChunkDurationMs() const {
  return kAudioPacketIntervalMs;
}

}  // namespace speech
