// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/extensions/api/system_info_cpu/system_info_cpu_api.h"

#include "base/logging.h"
#include "content/public/browser/browser_thread.h"
#include "chrome/browser/extensions/api/system_info_cpu/cpu_info_provider.h"

namespace extensions {
using api::experimental_system_info_cpu::CpuInfo;
using api::experimental_system_info_cpu::CpuCoreInfo;
using content::BrowserThread;

SystemInfoCpuGetFunction::SystemInfoCpuGetFunction() {
}

SystemInfoCpuGetFunction::~SystemInfoCpuGetFunction() {
  // Delete the provider that was created on FILE thread.
  BrowserThread::DeleteSoon(BrowserThread::FILE, FROM_HERE, provider_);
}

bool SystemInfoCpuGetFunction::RunImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
    base::Bind(&SystemInfoCpuGetFunction::WorkOnFileThread, this));
  return true;
}

void SystemInfoCpuGetFunction::WorkOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  provider_ = CpuInfoProvider::Create();
  GetCpuInfoOnFileThread();
}

void SystemInfoCpuGetFunction::GetCpuInfoOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  bool success = false;

  CpuInfo info;
  if (provider_ && provider_->GetCpuInfo(&info)) {
    SetResult(info.ToValue().release());
    success = true;
  } else {
    SetError("Error in querying cpu information!");
  }
  // Respond on UI thread.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&SystemInfoCpuGetFunction::RespondOnUIThread, this, success));
}


void SystemInfoCpuGetFunction::RespondOnUIThread(bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SendResponse(success);
}

// A mock implementation of CpuInfoProver for temporary usage.
// Will be moved out when the platform specific implementation are done.
class MockCpuInfoProvider : public CpuInfoProvider {
 public:
  MockCpuInfoProvider() {}
  virtual ~MockCpuInfoProvider() {}
  virtual bool GetCpuInfo(CpuInfo* info) OVERRIDE;
};

bool MockCpuInfoProvider::GetCpuInfo(CpuInfo* info) {
  if (!info) return false;
  linked_ptr<CpuCoreInfo> core(new CpuCoreInfo());
  core->load = 53;
  info->cores.push_back(core);
  return true;
}

// static
CpuInfoProvider* CpuInfoProvider::Create() {
  return new MockCpuInfoProvider();
}

}  // namespace extensions
