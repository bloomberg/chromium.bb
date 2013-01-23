// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/user_policy_token_loader.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/policy/enterprise_metrics.h"
#include "chrome/browser/policy/proto/device_management_local.pb.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

// Other places can sample on the same UMA counter, so make sure they all do
// it on the same thread (UI).
void SampleUMAOnUIThread(policy::MetricToken sample) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  UMA_HISTOGRAM_ENUMERATION(policy::kMetricToken, sample,
                            policy::kMetricTokenSize);
}

void SampleUMA(policy::MetricToken sample) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&SampleUMAOnUIThread, sample));
}

}  // namespace

namespace policy {

namespace em = enterprise_management;

UserPolicyTokenLoader::Delegate::~Delegate() {}

UserPolicyTokenLoader::UserPolicyTokenLoader(
    const base::WeakPtr<Delegate>& delegate,
    const FilePath& cache_file)
    : delegate_(delegate),
      cache_file_(cache_file) {}

void UserPolicyTokenLoader::Load() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&UserPolicyTokenLoader::LoadOnFileThread, this));
}

UserPolicyTokenLoader::~UserPolicyTokenLoader() {
}

void UserPolicyTokenLoader::LoadOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  std::string device_token;
  std::string device_id;

  if (file_util::PathExists(cache_file_)) {
    std::string data;
    em::DeviceCredentials device_credentials;
    if (file_util::ReadFileToString(cache_file_, &data) &&
        device_credentials.ParseFromArray(data.c_str(), data.size())) {
      device_token = device_credentials.device_token();
      device_id = device_credentials.device_id();
      SampleUMA(kMetricTokenLoadSucceeded);
    } else {
      SampleUMA(kMetricTokenLoadFailed);
    }
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&UserPolicyTokenLoader::NotifyOnUIThread,
                 this,
                 device_token,
                 device_id));
}

void UserPolicyTokenLoader::NotifyOnUIThread(const std::string& token,
                                             const std::string& device_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (delegate_.get())
    delegate_->OnTokenLoaded(token, device_id);
}

}  // namespace policy
