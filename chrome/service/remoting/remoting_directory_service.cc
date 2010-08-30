// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "chrome/service/net/service_url_request_context.h"
#include "chrome/service/remoting/remoting_directory_service.h"
#include "net/base/net_util.h"
#include "net/http/http_request_headers.h"
#include "remoting/host/host_key_pair.h"

static const char kRemotingDirectoryUrl[] =
    "https://www.googleapis.com/chromoting/v1/@me/hosts";

RemotingDirectoryService::RemotingDirectoryService(Client* client)
    : client_(client) {
}

RemotingDirectoryService::~RemotingDirectoryService() {
  DCHECK(!fetcher_.get()) << "URLFetcher not destroyed.";
}

void RemotingDirectoryService::AddHost(const std::string& token) {
  // TODO(hclam): This is a time consuming operation so we should run it on
  // a separate thread.
  host_key_pair_.reset(new remoting::HostKeyPair());
  host_key_pair_->Generate();

  // Use the host address as ID and host name.
  std::string hostname = net::GetHostName();
  host_id_ = hostname;
  host_name_ = hostname;

  // Prepare the parameters for the request.
  DictionaryValue data;
  data.SetString("hostId", hostname);
  data.SetString("hostName", hostname);
  data.SetString("publicKey", host_key_pair_->GetPublicKey());

  // Generate the final json query.
  DictionaryValue args;
  args.Set("data", data.DeepCopy());
  std::string request_content;
  base::JSONWriter::Write(&args, false, &request_content);

  // Prepare the HTTP header for authentication.
  net::HttpRequestHeaders headers;
  headers.SetHeader("Authorization", "GoogleLogin auth=" + token);
  fetcher_.reset(
      new URLFetcher(GURL(kRemotingDirectoryUrl), URLFetcher::POST, this));
  fetcher_->set_request_context(new ServiceURLRequestContextGetter());
  fetcher_->set_upload_data("application/json", request_content);
  fetcher_->set_extra_request_headers(headers.ToString());

  // And then start the request.
  fetcher_->Start();
}

void RemotingDirectoryService::CancelRequest() {
  fetcher_.reset();
}

void RemotingDirectoryService::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const URLRequestStatus& status,
    int response_code,
    const ResponseCookies& cookies,
    const std::string& data) {
  // Destroy the fetcher after the response has been received.
  fetcher_.reset();

  // TODO(hclam): Simply checking 200 status is not enough.
  if (response_code == 200) {
    client_->OnRemotingHostAdded();
  } else {
    client_->OnRemotingDirectoryError();
  }
}
