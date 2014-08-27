// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/gcp20/prototype/cloud_print_request.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

using net::URLFetcher;
using base::MessageLoop;

namespace {

const uint32 kDefaultTimeout = 20;  // in seconds

}  // namespace

CloudPrintRequest::CloudPrintRequest(const GURL& url,
                                     URLFetcher::RequestType method,
                                     Delegate* delegate)
    : fetcher_(URLFetcher::Create(url, method, this)),
      delegate_(delegate) {
  int load_flags = fetcher_->GetLoadFlags();
  load_flags |= net::LOAD_DO_NOT_SEND_COOKIES;
  load_flags |= net::LOAD_DO_NOT_SAVE_COOKIES;
  fetcher_->SetLoadFlags(load_flags);

  fetcher_->AddExtraRequestHeader("X-CloudPrint-Proxy: \"\"");
}

CloudPrintRequest::~CloudPrintRequest() {
}

scoped_ptr<CloudPrintRequest> CloudPrintRequest::CreateGet(
    const GURL& url,
    Delegate* delegate) {
  return scoped_ptr<CloudPrintRequest>(
      new CloudPrintRequest(url, URLFetcher::GET, delegate));
}

scoped_ptr<CloudPrintRequest> CloudPrintRequest::CreatePost(
    const GURL& url,
    const std::string& content,
    const std::string& mimetype,
    Delegate* delegate) {
  scoped_ptr<CloudPrintRequest> request(
      new CloudPrintRequest(url, URLFetcher::POST, delegate));
  request->fetcher_->SetUploadData(mimetype, content);
  return request.Pass();
}

void CloudPrintRequest::Run(
    const std::string& access_token,
    scoped_refptr<net::URLRequestContextGetter> context_getter) {
  if (!access_token.empty())
    fetcher_->AddExtraRequestHeader(base::StringPrintf(
        "Authorization: Bearer \"%s\"", access_token.c_str()));

  fetcher_->SetRequestContext(context_getter.get());
  fetcher_->Start();

  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&CloudPrintRequest::OnRequestTimeout, AsWeakPtr()),
      base::TimeDelta::FromSeconds(kDefaultTimeout));
}

void CloudPrintRequest::AddHeader(const std::string& header) {
  fetcher_->AddExtraRequestHeader(header);
}

void CloudPrintRequest::OnRequestTimeout() {
  if (!fetcher_)
    return;
  fetcher_.reset();
  LOG(WARNING) << "Request timeout reached.";

  DCHECK(delegate_);
  delegate_->OnFetchTimeoutReached();  // After this object can be deleted.
                                       // Do *NOT* access members after this
                                       // call.
}

void CloudPrintRequest::OnURLFetchComplete(const URLFetcher* source) {
  DCHECK(source == fetcher_.get());
  std::string response;
  source->GetResponseAsString(&response);

  int http_code = fetcher_->GetResponseCode();
  fetcher_.reset();
  VLOG(3) << response;

  DCHECK(delegate_);
  if (http_code == net::HTTP_OK) {
    delegate_->OnFetchComplete(response);  // After this object can be deleted.
                                           // Do *NOT* access members after
                                           // this call.

  } else {
    // TODO(maksymb): Add Privet |server_http_code| and |server_api| support in
    // case of server errors.
    NOTIMPLEMENTED() << "HTTP code: " << http_code;
    delegate_->OnFetchError("dummy", -1, http_code);  // After this object can
                                                      // be deleted.
                                                      // Do *NOT* access members
                                                      // after this call.
  }
}

