// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/iapps_finder_impl.h"

#include <string>

#include "base/base_paths_win.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/browser/media_galleries/fileapi/iapps_finder.h"
#include "chrome/browser/media_galleries/fileapi/safe_itunes_pref_parser_win.h"
#include "chrome/common/chrome_paths.h"
#include "components/storage_monitor/storage_info.h"
#include "content/public/browser/browser_thread.h"

namespace iapps {

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

// Check the default location for a correctly named file.
void TryDefaultLocation(const iapps::IAppsFinderCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::FILE);

  base::FilePath music_dir;
  if (!PathService::Get(chrome::DIR_USER_MUSIC, &music_dir)) {
    callback.Run(std::string());
    return;
  }
  base::FilePath library_file =
      music_dir.AppendASCII("iTunes").AppendASCII("iTunes Music Library.xml");

  if (!base::PathExists(library_file)) {
    callback.Run(std::string());
    return;
  }
  callback.Run(library_file.AsUTF8Unsafe());
}

// Check the location that parsing the preferences XML file found.
void FinishedParsingPrefXML(const iapps::IAppsFinderCallback& callback,
                            const base::FilePath& library_file) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::FILE);

  if (library_file.empty() || !base::PathExists(library_file)) {
    TryDefaultLocation(callback);
    return;
  }
  callback.Run(library_file.AsUTF8Unsafe());
}

void FindITunesLibraryOnFileThread(const iapps::IAppsFinderCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::FILE);

  std::string xml_pref_data = GetPrefFileData();
  if (xml_pref_data.empty()) {
    TryDefaultLocation(callback);
    return;
  }

  scoped_refptr<itunes::SafeITunesPrefParserWin> parser =
      new itunes::SafeITunesPrefParserWin(
          xml_pref_data,
          base::Bind(&FinishedParsingPrefXML, callback));
  parser->Start();
}

}  // namespace

// The Windows-specific ITunesFinder uses a utility process to parse the
// iTunes preferences XML file if it exists. If not or if the parsing fails,
// ITunesFinderWin will try a default location as well.
void FindITunesLibrary(const IAppsFinderCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  FindIAppsOnFileThread(storage_monitor::StorageInfo::ITUNES,
                        base::Bind(FindITunesLibraryOnFileThread), callback);
}

}  // namespace iapps
