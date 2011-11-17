// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/sdch_dictionary_fetcher.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/common/url_fetcher.h"
#include "net/url_request/url_request_status.h"

SdchDictionaryFetcher::SdchDictionaryFetcher()
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      task_is_pending_(false) {
}

SdchDictionaryFetcher::~SdchDictionaryFetcher() {
}

// static
void SdchDictionaryFetcher::Shutdown() {
  net::SdchManager::Shutdown();
}

void SdchDictionaryFetcher::Schedule(const GURL& dictionary_url) {
  // Avoid pushing duplicate copy onto queue.  We may fetch this url again later
  // and get a different dictionary, but there is no reason to have it in the
  // queue twice at one time.
  if (!fetch_queue_.empty() && fetch_queue_.back() == dictionary_url) {
    net::SdchManager::SdchErrorRecovery(
        net::SdchManager::DICTIONARY_ALREADY_SCHEDULED_TO_DOWNLOAD);
    return;
  }
  if (attempted_load_.find(dictionary_url) != attempted_load_.end()) {
    net::SdchManager::SdchErrorRecovery(
        net::SdchManager::DICTIONARY_ALREADY_TRIED_TO_DOWNLOAD);
    return;
  }
  attempted_load_.insert(dictionary_url);
  fetch_queue_.push(dictionary_url);
  ScheduleDelayedRun();
}

void SdchDictionaryFetcher::ScheduleDelayedRun() {
  if (fetch_queue_.empty() || current_fetch_.get() || task_is_pending_)
    return;
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      base::Bind(&SdchDictionaryFetcher::StartFetching,
      weak_factory_.GetWeakPtr()),
      kMsDelayFromRequestTillDownload);
  task_is_pending_ = true;
}

void SdchDictionaryFetcher::StartFetching() {
  DCHECK(task_is_pending_);
  task_is_pending_ = false;

  net::URLRequestContextGetter* context =
      Profile::Deprecated::GetDefaultRequestContext();
  if (!context) {
    // Shutdown in progress.
    // Simulate handling of all dictionary requests by clearing queue.
    while (!fetch_queue_.empty())
      fetch_queue_.pop();
    return;
  }

  current_fetch_.reset(content::URLFetcher::Create(
      fetch_queue_.front(), content::URLFetcher::GET, this));
  fetch_queue_.pop();
  current_fetch_->SetRequestContext(context);
  current_fetch_->Start();
}

void SdchDictionaryFetcher::OnURLFetchComplete(
    const content::URLFetcher* source) {
  if ((200 == source->GetResponseCode()) &&
      (source->GetStatus().status() == net::URLRequestStatus::SUCCESS)) {
    std::string data;
    source->GetResponseAsString(&data);
    net::SdchManager::Global()->AddSdchDictionary(data, source->GetURL());
  }
  current_fetch_.reset(NULL);
  ScheduleDelayedRun();
}
