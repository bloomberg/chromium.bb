// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/itunes_finder.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/storage_monitor/storage_info.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_WIN)
#include "chrome/browser/media_galleries/fileapi/itunes_finder_win.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/media_galleries/fileapi/itunes_finder_mac.h"
#endif

namespace itunes {

ITunesFinder::~ITunesFinder() {}

// static
void ITunesFinder::FindITunesLibrary(const ITunesFinderCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!callback.is_null());

  ITunesFinder* finder = NULL;
#if defined(OS_WIN)
  finder = new ITunesFinderWin(callback);
#elif defined(OS_MACOSX)
  finder = new ITunesFinderMac(callback);
#endif
  if (finder) {
    content::BrowserThread::PostTask(
        content::BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&ITunesFinder::FindITunesLibraryOnFileThread,
                   base::Unretained(finder)));
  } else {
    callback.Run(std::string());
  }
}

ITunesFinder::ITunesFinder(const ITunesFinderCallback& callback)
    : callback_(callback) {
}

void ITunesFinder::PostResultToUIThread(const std::string& unique_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  // The use of base::Owned() below will cause this class to get deleted either
  // when FinishOnUIThread() finishes or if the PostTask() fails.
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&ITunesFinder::FinishOnUIThread,
                 base::Owned(this),
                 unique_id));
}

void ITunesFinder::FinishOnUIThread(const std::string& unique_id) const {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  std::string device_id;
  if (!unique_id.empty())
    device_id = StorageInfo::MakeDeviceId(StorageInfo::ITUNES, unique_id);
  callback_.Run(device_id);
}

}  // namespace itunes
