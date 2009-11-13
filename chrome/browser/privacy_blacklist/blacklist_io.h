// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BLACKLIST_BLACKLIST_IO_H_
#define CHROME_BROWSER_PRIVACY_BLACKLIST_BLACKLIST_IO_H_

#include <string>

class Blacklist;
class FilePath;

// Set of routines to read and write blacklists.
class BlacklistIO {
 public:
  // Reads a blacklist stored on disk in a text format.
  // On error returns false and fills |error_string|.
  static bool ReadText(Blacklist* blacklist, const FilePath& path,
                       std::string* error_string);

  // Reads a blacklist stored on disk in a binary format.
  // Returns true on success.
  static bool ReadBinary(Blacklist* blacklist, const FilePath& path);

  // Writes |blacklist| to |path| in a binary format. Returns true on success.
  static bool WriteBinary(const Blacklist* blacklist, const FilePath& path);
};

#endif  // CHROME_BROWSER_PRIVACY_BLACKLIST_BLACKLIST_IO_H_
