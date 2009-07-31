// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_SAFARI_IMPORTER_H_
#define CHROME_BROWSER_IMPORTER_SAFARI_IMPORTER_H_

#include "chrome/browser/importer/importer.h"

#if __OBJC__
@class NSDictionary;
@class NSString;
#else
class NSDictionary;
class NSString;
#endif

// Importer for Safari on OS X.
class SafariImporter : public Importer {
 public:
  // |library_dir| is the full path to the ~/Library directory,
  // We pass it in as a parameter for testing purposes.
  explicit SafariImporter(const FilePath& library_dir);
  virtual ~SafariImporter();

  // Importer methods.
  virtual void StartImport(ProfileInfo profile_info,
                           uint16 items,
                           ProfileWriter* writer,
                           MessageLoop* delegate_loop,
                           ImporterHost* host);

 private:
  FRIEND_TEST(SafariImporterTest, HistoryImport);

  void ImportBookmarks();
  void ImportSearchEngines();
  void ImportPasswords();
  void ImportHistory();
  void ImportHomepage();

  // Converts history time stored by Safari as a double serialized as a string,
  // to seconds-since-UNIX-Ephoch-format used by Chrome.
  double HistoryTimeToEpochTime(NSString* history_time);

  // Parses Safari's history and loads it into the input array.
  void ParseHistoryItems(std::vector<history::URLRow>* history_items);

  // Given the URL of a page and a favicon data URL, adds an appropriate record
  // to the given favicon usage vector. Will do nothing if the favicon is not
  // valid.
  static void DataURLToFaviconUsage(
      const GURL& link_url,
      const GURL& favicon_data,
      std::vector<history::ImportedFavIconUsage>* favicons);

  ProfileWriter* writer_;
  FilePath library_dir_;

  DISALLOW_COPY_AND_ASSIGN(SafariImporter);
};

#endif  // CHROME_BROWSER_IMPORTER_SAFARI_IMPORTER_H_
