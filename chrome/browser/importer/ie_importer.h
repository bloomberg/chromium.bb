// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_IE_IMPORTER_H_
#define CHROME_BROWSER_IMPORTER_IE_IMPORTER_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/file_path.h"
#include "base/string16.h"
#include "chrome/browser/importer/importer.h"
#include "chrome/browser/importer/profile_writer.h"

class IEImporter : public Importer {
 public:
  IEImporter();

  // Importer:
  virtual void StartImport(const importer::SourceProfile& source_profile,
                           uint16 items,
                           ImporterBridge* bridge) OVERRIDE;

 private:
  typedef std::vector<ProfileWriter::BookmarkEntry> BookmarkVector;

  // A struct that hosts the information of IE Favorite folder.
  struct FavoritesInfo {
    FilePath path;
    string16 links_folder;
  };

  // IE PStore subkey GUID: AutoComplete password & form data.
  static const GUID kPStoreAutocompleteGUID;

  // A fake GUID for unit test.
  static const GUID kUnittestGUID;

  FRIEND_TEST_ALL_PREFIXES(ImporterTest, IEImporter);

  virtual ~IEImporter();

  void ImportFavorites();

  // Reads history information from COM interface.
  void ImportHistory();

  // Import password for IE6 stored in protected storage.
  void ImportPasswordsIE6();

  // Import password for IE7 and IE8 stored in Storage2.
  void ImportPasswordsIE7();

  void ImportSearchEngines();

  // Import the homepage setting of IE. Note: IE supports multiple home pages,
  // whereas Chrome doesn't, so we import only the one defined under the
  // 'Start Page' registry key. We don't import if the homepage is set to the
  // machine default.
  void ImportHomepage();

  // Resolves what's the .url file actually targets.
  // Returns empty string if failed.
  string16 ResolveInternetShortcut(const string16& file);

  // Gets the information of Favorites folder. Returns true if successful.
  bool GetFavoritesInfo(FavoritesInfo* info);

  // This function will read the files in the Favorite folder, and store
  // the bookmark items in |bookmarks|.
  void ParseFavoritesFolder(const FavoritesInfo& info,
                            BookmarkVector* bookmarks);

  // Determines which version of IE is in use.
  int CurrentIEVersion() const;

  // IE does not have source path. It's used in unit tests only for providing a
  // fake source.
  FilePath source_path_;

  DISALLOW_COPY_AND_ASSIGN(IEImporter);
};

#endif  // CHROME_BROWSER_IMPORTER_IE_IMPORTER_H_
