// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/system_logs/log_sources/memory_details_log_source.h"

#include "chrome/browser/memory_details.h"
#include "content/public/browser/browser_thread.h"

namespace system_logs {

// Reads Chrome memory usage.
class SystemLogsMemoryHandler : public MemoryDetails {
 public:
  explicit SystemLogsMemoryHandler(const SysLogsSourceCallback& callback)
      : callback_(callback) {}

  // Sends the data to the callback.
  // MemoryDetails override.
  virtual void OnDetailsAvailable() OVERRIDE {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

    scoped_ptr<SystemLogsResponse> response(new SystemLogsResponse);
    (*response)["mem_usage"] = ToLogString();
    callback_.Run(response.get());
  }

 private:
  virtual ~SystemLogsMemoryHandler() {}
  SysLogsSourceCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(SystemLogsMemoryHandler);
};

void MemoryDetailsLogSource::Fetch(const SysLogsSourceCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_refptr<SystemLogsMemoryHandler>
      handler(new SystemLogsMemoryHandler(callback));
  // TODO(jamescook): Maybe we don't need to update histograms here?
  handler->StartFetch(MemoryDetails::UPDATE_USER_METRICS);
}

}  // namespace system_logs
