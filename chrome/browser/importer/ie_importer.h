// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_IE_IMPORTER_H_
#define CHROME_BROWSER_IMPORTER_IE_IMPORTER_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/importer/importer_host.h"

class IEImporter : public Importer {
 public:
  IEImporter();

  // Importer:
  virtual void StartImport(const importer::ProfileInfo& browser_info,
                           uint16 items,
                           ImporterBridge* bridge) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(ImporterTest, IEImporter);

  virtual ~IEImporter();

  void ImportFavorites();
  void ImportHistory();

  // Import password for IE6 stored in protected storage.
  void ImportPasswordsIE6();

  // Import password for IE7 and IE8 stored in Storage2.
  void ImportPasswordsIE7();

  virtual void ImportSearchEngines();

  // Import the homepage setting of IE. Note: IE supports multiple home pages,
  // whereas Chrome doesn't, so we import only the one defined under the
  // 'Start Page' registry key. We don't import if the homepage is set to the
  // machine default.
  void ImportHomepage();

  // Resolves what's the .url file actually targets.
  // Returns empty string if failed.
  std::wstring ResolveInternetShortcut(const std::wstring& file);

  // A struct hosts the information of IE Favorite folder.
  struct FavoritesInfo {
    std::wstring path;
    std::wstring links_folder;
  };
  typedef std::vector<ProfileWriter::BookmarkEntry> BookmarkVector;

  // Gets the information of Favorites folder. Returns true if successful.
  bool GetFavoritesInfo(FavoritesInfo* info);

  // This function will read the files in the Favorite folder, and store
  // the bookmark items in |bookmarks|.
  void ParseFavoritesFolder(const FavoritesInfo& info,
                            BookmarkVector* bookmarks);

  // Determines which version of IE is in use.
  int CurrentIEVersion() const;

  // IE PStore subkey GUID: AutoComplete password & form data.
  static const GUID kPStoreAutocompleteGUID;

  // A fake GUID for unit test.
  static const GUID kUnittestGUID;

  // A struct hosts the information of AutoComplete data in PStore.
  struct AutoCompleteInfo {
    std::wstring key;
    std::vector<std::wstring> data;
    bool is_url;
  };

  // IE does not have source path. It's used in unit tests only for
  // providing a fake source.
  std::wstring source_path_;

  DISALLOW_COPY_AND_ASSIGN(IEImporter);
};

#endif  // CHROME_BROWSER_IMPORTER_IE_IMPORTER_H_
