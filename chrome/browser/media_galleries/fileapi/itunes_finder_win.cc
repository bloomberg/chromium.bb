// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/itunes_finder_win.h"

#include <string>

#include "base/base_paths_win.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/browser/media_galleries/fileapi/safe_itunes_pref_parser_win.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"

namespace itunes {

namespace {

// Try to read the iTunes preferences file from the default location and return
// its contents if found.
std::string GetPrefFileData() {
  std::string xml_pref_data;

  base::FilePath appdata_dir;
  if (PathService::Get(base::DIR_APP_DATA, &appdata_dir)) {
    base::FilePath pref_file = appdata_dir.AppendASCII("Apple Computer")
                                          .AppendASCII("iTunes")
                                          .AppendASCII("iTunesPrefs.xml");
    base::ReadFileToString(pref_file, &xml_pref_data);
  }
  return xml_pref_data;
}

}  // namespace

ITunesFinderWin::ITunesFinderWin(const ITunesFinderCallback& callback)
    : IAppFinder(StorageInfo::ITUNES, callback) {
}

ITunesFinderWin::~ITunesFinderWin() {}

void ITunesFinderWin::FindIAppOnFileThread() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  std::string xml_pref_data = GetPrefFileData();
  if (xml_pref_data.empty()) {
    TryDefaultLocation();
    return;
  }

  scoped_refptr<SafeITunesPrefParserWin> parser =
      new SafeITunesPrefParserWin(
          xml_pref_data,
          base::Bind(&ITunesFinderWin::FinishedParsingPrefXML,
                     base::Unretained(this)));
  parser->Start();
}

void ITunesFinderWin::TryDefaultLocation() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  base::FilePath music_dir;
  if (!PathService::Get(chrome::DIR_USER_MUSIC, &music_dir)) {
    PostResultToUIThread(std::string());
    return;
  }
  base::FilePath library_file =
      music_dir.AppendASCII("iTunes").AppendASCII("iTunes Music Library.xml");

  if (!base::PathExists(library_file)) {
    PostResultToUIThread(std::string());
    return;
  }
  PostResultToUIThread(library_file.AsUTF8Unsafe());
}

void ITunesFinderWin::FinishedParsingPrefXML(
    const base::FilePath& library_file) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));

  if (library_file.empty() || !base::PathExists(library_file)) {
    TryDefaultLocation();
    return;
  }
  PostResultToUIThread(library_file.AsUTF8Unsafe());
}

}  // namespace itunes
