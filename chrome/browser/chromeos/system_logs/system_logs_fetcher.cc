// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_logs/system_logs_fetcher.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/chromeos/system_logs/chrome_internal_log_source.h"
#include "chrome/browser/chromeos/system_logs/command_line_log_source.h"
#include "chrome/browser/chromeos/system_logs/dbus_log_source.h"
#include "chrome/browser/chromeos/system_logs/debug_daemon_log_source.h"
#include "chrome/browser/chromeos/system_logs/lsb_release_log_source.h"
#include "chrome/browser/chromeos/system_logs/memory_details_log_source.h"
#include "chrome/browser/chromeos/system_logs/network_event_log_source.h"
#include "chrome/browser/chromeos/system_logs/touch_log_source.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace chromeos {

SystemLogsFetcher::SystemLogsFetcher()
    : response_(new SystemLogsResponse),
      num_pending_requests_(0),
      weak_ptr_factory_(this) {
  // Debug Daemon data source.
  data_sources_.push_back(new DebugDaemonLogSource());

  // Chrome data sources.
  data_sources_.push_back(new ChromeInternalLogSource());
  data_sources_.push_back(new CommandLineLogSource());
  data_sources_.push_back(new DBusLogSource());
  data_sources_.push_back(new LsbReleaseLogSource());
  data_sources_.push_back(new MemoryDetailsLogSource());
  data_sources_.push_back(new NetworkEventLogSource());
  data_sources_.push_back(new TouchLogSource());

  num_pending_requests_ = data_sources_.size();
}

SystemLogsFetcher::~SystemLogsFetcher() {}

void SystemLogsFetcher::Fetch(const SysLogsFetcherCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(callback_.is_null());
  DCHECK(!callback.is_null());

  callback_ = callback;
  for (size_t i = 0; i < data_sources_.size(); ++i) {
    data_sources_[i]->Fetch(base::Bind(&SystemLogsFetcher::AddResponse,
                                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void SystemLogsFetcher::AddResponse(SystemLogsResponse* response) {
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

}  // namespace chromeos
