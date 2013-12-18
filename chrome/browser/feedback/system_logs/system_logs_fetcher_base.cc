// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/system_logs/system_logs_fetcher_base.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace system_logs {

SystemLogsFetcherBase::SystemLogsFetcherBase()
    : response_(new SystemLogsResponse),
      num_pending_requests_(0) {
}

SystemLogsFetcherBase::~SystemLogsFetcherBase() {}

void SystemLogsFetcherBase::Fetch(const SysLogsFetcherCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(callback_.is_null());
  DCHECK(!callback.is_null());

  callback_ = callback;
  for (size_t i = 0; i < data_sources_.size(); ++i) {
    data_sources_[i]->Fetch(base::Bind(&SystemLogsFetcherBase::AddResponse,
                                       AsWeakPtr()));
  }
}

void SystemLogsFetcherBase::AddResponse(SystemLogsResponse* response) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  for (SystemLogsResponse::const_iterator it = response->begin();
       it != response->end();
       ++it) {
    // It is an error to insert an element with a pre-existing key.
    bool ok = response_->insert(*it).second;
    DCHECK(ok) << "Duplicate key found: " << it->first;
  }

  --num_pending_requests_;
  if (num_pending_requests_ > 0)
    return;

  callback_.Run(response_.Pass());
  BrowserThread::DeleteSoon(BrowserThread::UI, FROM_HERE, this);
}

}  // namespace system_logs
