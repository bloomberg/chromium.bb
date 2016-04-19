// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/system_logs/log_sources/memory_details_log_source.h"

#include "base/macros.h"
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
  void OnDetailsAvailable() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    std::unique_ptr<SystemLogsResponse> response(new SystemLogsResponse);
    (*response)["mem_usage"] = ToLogString();
    callback_.Run(response.get());
  }

 private:
  ~SystemLogsMemoryHandler() override {}
  SysLogsSourceCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(SystemLogsMemoryHandler);
};

MemoryDetailsLogSource::MemoryDetailsLogSource()
    : SystemLogsSource("MemoryDetails") {
}

MemoryDetailsLogSource::~MemoryDetailsLogSource() {
}

void MemoryDetailsLogSource::Fetch(const SysLogsSourceCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());

  scoped_refptr<SystemLogsMemoryHandler>
      handler(new SystemLogsMemoryHandler(callback));
  handler->StartFetch();
}

}  // namespace system_logs
