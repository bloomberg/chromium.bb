// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_ITUNES_FINDER_WIN_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_ITUNES_FINDER_WIN_H_

#include "chrome/browser/media_galleries/fileapi/iapp_finder.h"
#include "chrome/browser/media_galleries/fileapi/itunes_finder.h"

namespace base {
class FilePath;
}

namespace itunes {

// This Windows-specific ITunesFinder uses a utility process to parse the
// iTunes preferences XML file if it exists. If not or if the parsing fails,
// ITunesFinderWin will try a default location as well.
class ITunesFinderWin : public iapps::IAppFinder {
 public:
  explicit ITunesFinderWin(const ITunesFinderCallback& callback);
  virtual ~ITunesFinderWin();

 private:
  virtual void FindIAppOnFileThread() OVERRIDE;

  // Check the default location for the iTunes library XML file.
  // Runs on the FILE thread.
  void TryDefaultLocation();

  // Called by SafeITunesPrefParser when it finishes parsing the preferences
  // XML file. Runs on the FILE thread.
  void FinishedParsingPrefXML(const base::FilePath& library_file);

  DISALLOW_COPY_AND_ASSIGN(ITunesFinderWin);
};

}  // namespace itunes

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_ITUNES_FINDER_WIN_H_
