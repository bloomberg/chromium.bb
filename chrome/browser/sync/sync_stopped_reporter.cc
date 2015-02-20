// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_stopped_reporter.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "sync/protocol/sync.pb.h"

namespace {
  const char kEventEndpoint[] = "event";

  // The request is tiny, so even on poor connections 10 seconds should be
  // plenty of time. Since sync is off when this request is started, we don't
  // want anything sync-related hanging around for very long from a human
  // perspective either. This seems like a good compromise.
  const base::TimeDelta kRequestTimeout = base::TimeDelta::FromSeconds(10);
}

namespace browser_sync {

SyncStoppedReporter::SyncStoppedReporter(
    const GURL& sync_service_url,
    const scoped_refptr<net::URLRequestContextGetter>& request_context,
    const ResultCallback& callback)
    : sync_event_url_(GetSyncEventURL(sync_service_url)),
      request_context_(request_context),
      callback_(callback) {
  DCHECK(!sync_service_url.is_empty());
  DCHECK(request_context);
}

SyncStoppedReporter::~SyncStoppedReporter() {}

void SyncStoppedReporter::ReportSyncStopped(const std::string& access_token,
                                            const std::string& cache_guid,
                                            const std::string& birthday) {
  DCHECK(!access_token.empty());
  DCHECK(!cache_guid.empty());
  DCHECK(!birthday.empty());

  // Make the request proto with the GUID identifying this client.
  sync_pb::EventRequest event_request;
  sync_pb::SyncDisabledEvent* sync_disabled_event =
      event_request.mutable_sync_disabled();
  sync_disabled_event->set_cache_guid(cache_guid);
  sync_disabled_event->set_store_birthday(birthday);

  std::string msg;
  event_request.SerializeToString(&msg);

  fetcher_.reset(net::URLFetcher::Create(
      sync_event_url_, net::URLFetcher::POST, this));
  fetcher_->AddExtraRequestHeader(base::StringPrintf(
      "%s: Bearer %s", net::HttpRequestHeaders::kAuthorization,
      access_token.c_str()));
  fetcher_->SetRequestContext(request_context_.get());
  fetcher_->SetUploadData("application/octet-stream", msg);
  fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES);
  fetcher_->Start();
  timer_.Start(FROM_HERE, kRequestTimeout, this,
               &SyncStoppedReporter::OnTimeout);
}

void SyncStoppedReporter::OnURLFetchComplete(const net::URLFetcher* source) {
  Result result = source->GetResponseCode() == net::HTTP_OK
      ? RESULT_SUCCESS : RESULT_ERROR;
  fetcher_.reset();
  timer_.Stop();
  if (!callback_.is_null()) {
    base::MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(callback_, result));
  }
}

void SyncStoppedReporter::OnTimeout() {
  fetcher_.reset();
  if (!callback_.is_null()) {
    base::MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(callback_, RESULT_TIMEOUT));
  }
}

// Static.
GURL SyncStoppedReporter::GetSyncEventURL(const GURL& sync_service_url) {
  std::string path = sync_service_url.path();
  if (path.empty() || *path.rbegin() != '/') {
    path += '/';
  }
  path += kEventEndpoint;
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  return sync_service_url.ReplaceComponents(replacements);
}

void SyncStoppedReporter::SetTimerTaskRunnerForTest(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  timer_.SetTaskRunner(task_runner);
}

}  // namespace browser_sync
