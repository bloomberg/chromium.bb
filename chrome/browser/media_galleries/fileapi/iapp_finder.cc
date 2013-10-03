// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/iapp_finder.h"

#include "base/bind.h"
#include "base/logging.h"
#include "content/public/browser/browser_thread.h"

namespace iapps {

IAppFinder::~IAppFinder() {}

IAppFinder::IAppFinder(StorageInfo::Type type,
                       const IAppFinderCallback& callback)
    : type_(type),
      callback_(callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!callback.is_null());

  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&IAppFinder::FindIAppOnFileThread, base::Unretained(this)));
}

void IAppFinder::PostResultToUIThread(const std::string& unique_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  // The use of base::Owned() below will cause this class to get deleted either
  // when FinishOnUIThread() finishes or if the PostTask() fails.
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&IAppFinder::FinishOnUIThread,
                 base::Owned(this),
                 unique_id));
}

void IAppFinder::FinishOnUIThread(const std::string& unique_id) const {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  std::string device_id;
  if (!unique_id.empty())
    device_id = StorageInfo::MakeDeviceId(type_, unique_id);
  callback_.Run(device_id);
}

}  // namespace iapps
