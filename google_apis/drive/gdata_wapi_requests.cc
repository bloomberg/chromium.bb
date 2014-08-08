// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/drive/gdata_wapi_requests.h"

#include "base/location.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "base/values.h"
#include "google_apis/drive/gdata_wapi_parser.h"
#include "google_apis/drive/gdata_wapi_url_generator.h"

namespace google_apis {

namespace {

scoped_ptr<ResourceEntry> ParseResourceEntry(const std::string& json) {
  scoped_ptr<base::Value> value = ParseJson(json);
  return value ? ResourceEntry::ExtractAndParse(*value) :
      scoped_ptr<ResourceEntry>();
}

}  // namespace

GetResourceEntryRequest::GetResourceEntryRequest(
    RequestSender* sender,
    const GDataWapiUrlGenerator& url_generator,
    const std::string& resource_id,
    const GURL& embed_origin,
    const GetResourceEntryCallback& callback)
    : UrlFetchRequestBase(sender),
      url_generator_(url_generator),
      resource_id_(resource_id),
      embed_origin_(embed_origin),
      callback_(callback),
      weak_ptr_factory_(this) {
  DCHECK(!callback.is_null());
}

GetResourceEntryRequest::~GetResourceEntryRequest() {}

GURL GetResourceEntryRequest::GetURL() const {
  return url_generator_.GenerateEditUrlWithEmbedOrigin(
      resource_id_, embed_origin_);
}

void GetResourceEntryRequest::ProcessURLFetchResults(
    const net::URLFetcher* source) {
  GDataErrorCode error = GetErrorCode();
  switch (error) {
    case HTTP_SUCCESS:
    case HTTP_CREATED:
      base::PostTaskAndReplyWithResult(
          blocking_task_runner(),
          FROM_HERE,
          base::Bind(&ParseResourceEntry, response_writer()->data()),
          base::Bind(&GetResourceEntryRequest::OnDataParsed,
                     weak_ptr_factory_.GetWeakPtr(), error));
      break;
    default:
      RunCallbackOnPrematureFailure(error);
      OnProcessURLFetchResultsComplete();
      break;
  }
}

void GetResourceEntryRequest::RunCallbackOnPrematureFailure(
    GDataErrorCode error) {
  callback_.Run(error, scoped_ptr<ResourceEntry>());
}

void GetResourceEntryRequest::OnDataParsed(GDataErrorCode error,
                                           scoped_ptr<ResourceEntry> entry) {
  if (!entry)
    error = GDATA_PARSE_ERROR;
  callback_.Run(error, entry.Pass());
  OnProcessURLFetchResultsComplete();
}

}  // namespace google_apis
