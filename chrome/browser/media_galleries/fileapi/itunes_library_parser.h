// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_ITUNES_LIBRARY_PARSER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_ITUNES_LIBRARY_PARSER_H_

#include <map>
#include <set>

#include "base/files/file_path.h"

namespace itunes {

class ITunesLibraryParser {
 public:
  struct Track {
    Track(uint64 id, const base::FilePath& location);
    bool operator<(const Track& other) const;

    uint64 id;
    base::FilePath location;
  };

  typedef std::set<Track> Album;
  typedef std::map<std::string /*album name*/, Album> Albums;
  typedef std::map<std::string /*artist name*/, Albums> Library;

  ITunesLibraryParser();
  ~ITunesLibraryParser();

  // Returns true if at least one track was found. Malformed track entries
  // are silently ignored.
  bool Parse(const std::string& xml);

  const Library& library() { return library_; }

 private:
  Library library_;

  DISALLOW_COPY_AND_ASSIGN(ITunesLibraryParser);
};

}  // namespace itunes

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_ITUNES_LIBRARY_PARSER_H_
