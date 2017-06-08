// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/system_logs/system_logs_fetcher.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace system_logs {

SystemLogsSource::SystemLogsSource(const std::string& source_name)
    : source_name_(source_name) {}

SystemLogsSource::~SystemLogsSource() {}

SystemLogsFetcher::SystemLogsFetcher(bool scrub_data)
    : response_(base::MakeUnique<SystemLogsResponse>()),
      num_pending_requests_(0),
      weak_ptr_factory_(this) {
  if (scrub_data)
    anonymizer_ = base::MakeUnique<feedback::AnonymizerTool>();
}

SystemLogsFetcher::~SystemLogsFetcher() {}

void SystemLogsFetcher::AddSource(std::unique_ptr<SystemLogsSource> source) {
  data_sources_.emplace_back(std::move(source));
  num_pending_requests_++;
}

void SystemLogsFetcher::Fetch(const SysLogsFetcherCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(callback_.is_null());
  DCHECK(!callback.is_null());

  callback_ = callback;
  for (size_t i = 0; i < data_sources_.size(); ++i) {
    VLOG(1) << "Fetching SystemLogSource: " << data_sources_[i]->source_name();
    data_sources_[i]->Fetch(base::Bind(&SystemLogsFetcher::OnFetched,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       data_sources_[i]->source_name()));
  }
}

void SystemLogsFetcher::OnFetched(const std::string& source_name,
                                  SystemLogsResponse* response) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  VLOG(1) << "Received SystemLogSource: " << source_name;

  if (anonymizer_) {
    for (auto& element : *response)
      element.second = anonymizer_->Anonymize(element.second);
  }
  AddResponse(source_name, response);
}

void SystemLogsFetcher::AddResponse(const std::string& source_name,
                                    SystemLogsResponse* response) {
  for (const auto& it : *response) {
    // An element with a duplicate key would not be successfully inserted.
    bool ok = response_->emplace(it).second;
    DCHECK(ok) << "Duplicate key found: " << it.first;
  }

  --num_pending_requests_;
  if (num_pending_requests_ > 0)
    return;

  callback_.Run(std::move(response_));
  BrowserThread::DeleteSoon(BrowserThread::UI, FROM_HERE, this);
}

}  // namespace system_logs
