// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_recognition_request.h"

#include <vector>

#include "app/l10n_util.h"
#include "base/json/json_reader.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_status.h"

namespace {

const char* const kDefaultSpeechRecognitionUrl =
    "https://www.google.com/speech-api/v1/recognize?client=chromium&";
const char* const kHypothesesString = "hypotheses";
const char* const kUtteranceString = "utterance";
const char* const kConfidenceString = "confidence";

bool ParseServerResponse(const std::string& response_body,
                         speech_input::SpeechInputResultArray* result) {
  if (response_body.empty()) {
    LOG(WARNING) << "ParseServerResponse: Response was empty.";
    return false;
  }
  DVLOG(1) << "ParseServerResponse: Parsing response " << response_body;

  // Parse the response, ignoring comments.
  std::string error_msg;
  scoped_ptr<Value> response_value(base::JSONReader::ReadAndReturnError(
      response_body, false, NULL, &error_msg));
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

  // Get the hypotheses
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
  if (hypotheses_list->GetSize() == 0) {
    VLOG(1) << "ParseServerResponse: hypotheses list is empty.";
    return false;
  }

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
    hypothesis_value->GetReal(kConfidenceString, &confidence);

    result->push_back(speech_input::SpeechInputResultItem(utterance,
                                                          confidence));
  }

  if (index < hypotheses_list->GetSize()) {
    result->clear();
    return false;
  }

  return true;
}

}  // namespace

namespace speech_input {

int SpeechRecognitionRequest::url_fetcher_id_for_tests = 0;

SpeechRecognitionRequest::SpeechRecognitionRequest(
    URLRequestContextGetter* context, Delegate* delegate)
    : url_context_(context),
      delegate_(delegate) {
  DCHECK(delegate);
}

SpeechRecognitionRequest::~SpeechRecognitionRequest() {}

bool SpeechRecognitionRequest::Send(const std::string& language,
                                    const std::string& grammar,
                                    const std::string& hardware_info,
                                    const std::string& origin_url,
                                    const std::string& content_type,
                                    const std::string& audio_data) {
  DCHECK(!url_fetcher_.get());

  std::vector<std::string> parts;

  std::string lang_param = language;
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
  parts.push_back("lang=" + EscapeQueryParamValue(lang_param, true));

  if (!grammar.empty())
    parts.push_back("lm=" + EscapeQueryParamValue(grammar, true));
  if (!hardware_info.empty())
    parts.push_back("xhw=" + EscapeQueryParamValue(hardware_info, true));
  // TODO(satish): Remove this hardcoded value once the page is allowed to
  // set this via an attribute.
  parts.push_back("maxresults=3");

  GURL url(std::string(kDefaultSpeechRecognitionUrl) + JoinString(parts, '&'));

  url_fetcher_.reset(URLFetcher::Create(url_fetcher_id_for_tests,
                                        url,
                                        URLFetcher::POST,
                                        this));
  url_fetcher_->set_upload_data(content_type, audio_data);
  url_fetcher_->set_request_context(url_context_);
  url_fetcher_->set_referrer(origin_url);

  // The speech recognition API does not require user identification as part
  // of requests, so we don't send cookies or auth data for these requests to
  // prevent any accidental connection between users who are logged into the
  // domain for other services (e.g. bookmark sync) with the speech requests.
  url_fetcher_->set_load_flags(
      net::LOAD_DO_NOT_SAVE_COOKIES | net::LOAD_DO_NOT_SEND_COOKIES |
      net::LOAD_DO_NOT_SEND_AUTH_DATA);
  url_fetcher_->Start();
  return true;
}

void SpeechRecognitionRequest::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  DCHECK_EQ(url_fetcher_.get(), source);

  bool error = !status.is_success() || response_code != 200;
  SpeechInputResultArray result;
  if (!error)
    error = !ParseServerResponse(data, &result);
  url_fetcher_.reset();

  DVLOG(1) << "SpeechRecognitionRequest: Invoking delegate with result.";
  delegate_->SetRecognitionResult(error, result);
}

}  // namespace speech_input
