// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_recognition_request.h"

#include "base/json/json_reader.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_status.h"

namespace {

const char* const kHypothesesString = "hypotheses";
const char* const kUtteranceString = "utterance";

bool ParseServerResponse(const std::string& response_body, string16* value) {
  DCHECK(value);

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

  Value* first_hypotheses = NULL;
  if (!hypotheses_list->Get(0, &first_hypotheses)) {
    LOG(WARNING) << "ParseServerResponse: Unable to read hypotheses value.";
    return false;
  }
  DCHECK(first_hypotheses);
  if (!first_hypotheses->IsType(Value::TYPE_DICTIONARY)) {
    LOG(WARNING) << "ParseServerResponse: Unexpected value type "
                 << first_hypotheses->GetType();
    return false;
  }
  const DictionaryValue* first_hypotheses_value =
      static_cast<DictionaryValue*>(first_hypotheses);
  if (!first_hypotheses_value->GetString(kUtteranceString, value)) {
    LOG(WARNING) << "ParseServerResponse: Missing utterance value.";
    return false;
  }

  return true;
}

}  // namespace

namespace speech_input {

int SpeechRecognitionRequest::url_fetcher_id_for_tests = 0;

SpeechRecognitionRequest::SpeechRecognitionRequest(
    URLRequestContextGetter* context, const GURL& url, Delegate* delegate)
    : url_context_(context),
      url_(url),
      delegate_(delegate) {
  DCHECK(delegate);
}

bool SpeechRecognitionRequest::Send(const std::string& content_type,
                                    const std::string& audio_data) {
  DCHECK(!url_fetcher_.get());

  url_fetcher_.reset(URLFetcher::Create(
      url_fetcher_id_for_tests, url_, URLFetcher::POST, this));
  url_fetcher_->set_upload_data(content_type, audio_data);
  url_fetcher_->set_request_context(url_context_);

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
    const URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  DCHECK_EQ(url_fetcher_.get(), source);
  DCHECK(url_.possibly_invalid_spec() == url.possibly_invalid_spec());

  bool error = !status.is_success() || response_code != 200;
  string16 value;
  if (!error)
    error = !ParseServerResponse(data, &value);
  url_fetcher_.reset();

  DVLOG(1) << "SpeechRecognitionRequest: Invoking delegate with result.";
  delegate_->SetRecognitionResult(error, value);
}

}  // namespace speech_input
