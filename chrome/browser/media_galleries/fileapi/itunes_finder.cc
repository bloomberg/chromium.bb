// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/itunes_finder.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/media_galleries/fileapi/itunes_finder_win.h"
#include "content/public/browser/browser_thread.h"

namespace itunes {

ITunesFinder::~ITunesFinder() {}

// static
void ITunesFinder::FindITunesLibrary(ITunesFinderCallback callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  ITunesFinder* finder = NULL;
#if defined(OS_WIN)
  finder = new ITunesFinderWin(callback);
#endif
  if (finder) {
    content::BrowserThread::PostTask(
        content::BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&ITunesFinder::FindITunesLibraryOnFileThread,
                   base::Unretained(finder)));
  }
}

ITunesFinder::ITunesFinder(ITunesFinderCallback callback)
    : callback_(callback) {
}

void ITunesFinder::PostResultToUIThread(const std::string& result) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  result_ = result;
  // The use of base::Owned below will cause this class to delete as soon
  // as FinishOnUIThread finishes.
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&ITunesFinder::FinishOnUIThread, base::Owned(this)));
}

void ITunesFinder::FinishOnUIThread() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!result_.empty())
    callback_.Run(result_);
}

}  // namespace itunes
