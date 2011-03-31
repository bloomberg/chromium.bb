// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/remoting/directory_add_request.h"

#include <vector>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/common/net/http_return.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

namespace remoting {

static const char kRemotingDirectoryUrl[] =
    "https://www.googleapis.com/chromoting/v1/@me/hosts";

DirectoryAddRequest::DirectoryAddRequest(net::URLRequestContextGetter* getter)
    : getter_(getter) {
}

DirectoryAddRequest::~DirectoryAddRequest() {
  DCHECK(!fetcher_.get()) << "URLFetcher not destroyed.";
}

void DirectoryAddRequest::AddHost(const remoting::ChromotingHostInfo& host_info,
                                  const std::string& auth_token,
                                  DoneCallback* done_callback) {
  DCHECK(done_callback);
  done_callback_.reset(done_callback);

  // Prepare the parameters for the request.
  DictionaryValue data;
  data.SetString("hostId", host_info.host_id);
  data.SetString("hostName", host_info.hostname);
  data.SetString("publicKey", host_info.public_key);

  // Generate the final json query.
  DictionaryValue args;
  args.Set("data", data.DeepCopy());
  std::string request_content;
  base::JSONWriter::Write(&args, false, &request_content);

  // Prepare the HTTP header for authentication.
  net::HttpRequestHeaders headers;
  headers.SetHeader("Authorization", "GoogleLogin auth=" + auth_token);
  fetcher_.reset(
      new URLFetcher(GURL(kRemotingDirectoryUrl), URLFetcher::POST, this));
  fetcher_->set_request_context(getter_);
  fetcher_->set_upload_data("application/json", request_content);
  fetcher_->set_extra_request_headers(headers.ToString());

  // And then start the request.
  fetcher_->Start();
}

void DirectoryAddRequest::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  DCHECK_EQ(source, fetcher_.get());

  // Destroy the fetcher after the response has been received.
  fetcher_.reset();

  Result result;
  std::string error_message;

  if (status.is_success()) {
    DictionaryValue* response = NULL;
    scoped_ptr<Value> response_json(base::JSONReader::Read(data, true));
    if (response_json != NULL &&
        response_json->IsType(Value::TYPE_DICTIONARY)) {
      response = static_cast<DictionaryValue*>(response_json.get());
      response->GetString("error.message", &error_message);
    }

    switch (response_code) {
      case RC_REQUEST_OK:
        result = SUCCESS;
        break;

      case RC_BAD_REQUEST:
        // TODO(sergeyu): Implement duplicate error detection that doesn't
        // depend on error message.
        if (error_message.find("duplicate") != std::string::npos) {
          result = ERROR_EXISTS;
        } else {
          result = ERROR_INVALID_REQUEST;
        }
        break;

      case RC_UNAUTHORIZED:
        result = ERROR_AUTH;
        break;

      case RC_INTERNAL_SERVER_ERROR:
        result = ERROR_SERVER;
        break;

      default:
        result = ERROR_OTHER;
    }
  } else {
    result = ERROR_OTHER;
  }

  if (result != SUCCESS) {
    LOG(WARNING) << "Received error when trying to register Chromoting host. "
                 << "status.is_success(): " << status.is_success()
                 << "  response_code: " << response_code
                 << "  error_message: " << error_message;
  }

  done_callback_->Run(result, error_message);
}

}  // namespace remoting
