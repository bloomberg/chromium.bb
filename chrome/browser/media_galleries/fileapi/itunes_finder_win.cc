// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/itunes_finder_win.h"

#include <string>

#include "base/base64.h"
#include "base/base_paths_win.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "third_party/libxml/chromium/libxml_utils.h"

namespace itunes {

namespace {

// Traverse |reader| looking for a node named |name| at the current depth
// of |reader|.
bool SeekToNodeAtCurrentDepth(XmlReader* reader, const std::string& name) {
  int depth = reader->Depth();
  do {
    if (!reader->SkipToElement()) {
      // SkipToElement returns false if the current node is an end element,
      // try to advance to the next element and then try again.
      if (!reader->Read() || !reader->SkipToElement())
        return false;
    }
    DCHECK_EQ(depth, reader->Depth());
    if (reader->NodeName() == name)
      return true;
  } while (reader->Next());

  return false;
}

// Search within the dict for |key|.
bool SeekInDict(XmlReader* reader, const std::string& key) {
  DCHECK_EQ("dict", reader->NodeName());

  int dict_content_depth = reader->Depth() + 1;
  // Advance past the dict node and into the body of the dictionary.
  if (!reader->Read())
    return false;

  while (reader->Depth() >= dict_content_depth) {
    if (!SeekToNodeAtCurrentDepth(reader, "key"))
      return false;
    std::string found_key;
    if (!reader->ReadElementContent(&found_key))
      return false;
    DCHECK_EQ(dict_content_depth, reader->Depth());
    if (found_key == key)
      return true;
  }
  return false;
}

// Read the iTunes preferences from |pref_file| and then try to extract the
// library XML location from the XML file. Return it if found. The minimal
// valid snippet of XML is:
//
//   <plist>
//     <dict>
//       <key>User Preferences</key>
//       <dict>
//         <key>iTunes Library XML Location:1</key>
//         <data>Base64 encoded w string path</data>
//       </dict>
//     </dict>
//   </plist>
//
base::FilePath::StringType FindLibraryLocationInPrefXML(
    const std::string& pref_file) {
  XmlReader reader;
  base::FilePath::StringType result;

  if (!reader.LoadFile(pref_file))
    return result;

  // Find the plist node and then search within that tag.
  if (!SeekToNodeAtCurrentDepth(&reader, "plist"))
    return result;
  if (!reader.Read())
    return result;

  if (!SeekToNodeAtCurrentDepth(&reader, "dict"))
    return result;

  if (!SeekInDict(&reader, "User Preferences"))
    return result;

  if (!SeekToNodeAtCurrentDepth(&reader, "dict"))
    return result;

  if (!SeekInDict(&reader, "iTunes Library XML Location:1"))
    return result;

  if (!SeekToNodeAtCurrentDepth(&reader, "data"))
    return result;

  std::string pref_value;
  if (!reader.ReadElementContent(&pref_value))
    return result;
  // The data is a base64 encoded wchar_t*. Because Base64Decode uses
  // std::strings, the result has to be casted to a wchar_t*.
  std::string data;
  if (!base::Base64Decode(CollapseWhitespaceASCII(pref_value, true), &data))
    return result;
  return base::FilePath::StringType(
      reinterpret_cast<const wchar_t*>(data.data()), data.size()/2);
}

// Try to find the iTunes library XML by examining the iTunes preferences
// file and return it if found.
base::FilePath TryPreferencesFile() {
  base::FilePath appdata_dir;
  if (!PathService::Get(base::DIR_APP_DATA, &appdata_dir))
    return base::FilePath();
  base::FilePath pref_file = appdata_dir.AppendASCII("Apple Computer")
                                        .AppendASCII("iTunes")
                                        .AppendASCII("iTunesPrefs.xml");
  if (!file_util::PathExists(pref_file))
    return base::FilePath();

  base::FilePath::StringType library_location =
      FindLibraryLocationInPrefXML(pref_file.AsUTF8Unsafe());
  base::FilePath library_file(library_location);
  if (!file_util::PathExists(library_file))
    return base::FilePath();
  return library_file;
}

// Check the default location for the iTunes library XML file. Return the
// location if found.
base::FilePath TryDefaultLocation() {
  base::FilePath music_dir;
  if (!PathService::Get(chrome::DIR_USER_MUSIC, &music_dir))
    return base::FilePath();
  base::FilePath library_file =
      music_dir.AppendASCII("iTunes").AppendASCII("iTunes Music Library.xml");

  if (!file_util::PathExists(library_file))
    return base::FilePath();
  return library_file;
}

}  // namespace

ITunesFinderWin::ITunesFinderWin(const ITunesFinderCallback& callback)
    : ITunesFinder(callback) {
}

ITunesFinderWin::~ITunesFinderWin() {}

void ITunesFinderWin::FindITunesLibraryOnFileThread() {
  base::FilePath library_file = TryPreferencesFile();

  if (library_file.empty())
    library_file = TryDefaultLocation();

  if (library_file.empty()) {
    PostResultToUIThread(std::string());
    return;
  }

  PostResultToUIThread(library_file.AsUTF8Unsafe());
}

}  // namespace itunes
