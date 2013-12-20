// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_IMPORTER_IE_IMPORTER_WIN_H_
#define CHROME_UTILITY_IMPORTER_IE_IMPORTER_WIN_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/strings/string16.h"
#include "chrome/utility/importer/importer.h"

struct ImportedBookmarkEntry;
struct ImportedFaviconUsage;

class IEImporter : public Importer {
 public:
  IEImporter();

  // Importer:
  virtual void StartImport(const importer::SourceProfile& source_profile,
                           uint16 items,
                           ImporterBridge* bridge) OVERRIDE;

 private:
  typedef std::vector<ImportedBookmarkEntry> BookmarkVector;

  // A struct that hosts the information of IE Favorite folder.
  struct FavoritesInfo {
    base::FilePath path;
    base::string16 links_folder;
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

  // Gets the information of Favorites folder. Returns true if successful.
  bool GetFavoritesInfo(FavoritesInfo* info);

  // This function will read the files in the Favorites folder, and store
  // the bookmark items in |bookmarks| and favicon information in |favicons|.
  void ParseFavoritesFolder(
      const FavoritesInfo& info,
      BookmarkVector* bookmarks,
      std::vector<ImportedFaviconUsage>* favicons);

  // Determines which version of IE is in use.
  int CurrentIEVersion() const;

  // IE does not have source path. It's used in unit tests only for providing a
  // fake source.
  base::FilePath source_path_;

  DISALLOW_COPY_AND_ASSIGN(IEImporter);
};

#endif  // CHROME_UTILITY_IMPORTER_IE_IMPORTER_WIN_H_
