// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/games/core/games_prefs.h"

namespace games {
namespace prefs {

namespace {

const char kGamesCatalogPathPref[] = "games.data_files_paths.games-catalog";
const char kHighlightedGamesPathPref[] =
    "games.data_files_paths.highlighted-games";

void SetFilePathPref(PrefService* prefs,
                     const std::string& pref_path,
                     const base::FilePath& file_path) {
  prefs->SetFilePath(pref_path, file_path);
}

bool TryGetFilePathPref(PrefService* prefs,
                        const std::string& pref_path,
                        base::FilePath* out_file_path) {
  *out_file_path = prefs->GetFilePath(pref_path);
  return !out_file_path->empty();
}

}  // namespace

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterFilePathPref(kGamesCatalogPathPref, base::FilePath());
  registry->RegisterFilePathPref(kHighlightedGamesPathPref, base::FilePath());
}

void SetGamesCatalogPath(PrefService* prefs, const base::FilePath& file_path) {
  SetFilePathPref(prefs, kGamesCatalogPathPref, file_path);
}

void SetHighlightedGamesPath(PrefService* prefs,
                             const base::FilePath& file_path) {
  SetFilePathPref(prefs, kHighlightedGamesPathPref, file_path);
}

bool TryGetGamesCatalogPath(PrefService* prefs, base::FilePath* out_file_path) {
  return TryGetFilePathPref(prefs, kGamesCatalogPathPref, out_file_path);
}

bool TryGetHighlightedGamesPath(PrefService* prefs,
                                base::FilePath* out_file_path) {
  return TryGetFilePathPref(prefs, kHighlightedGamesPathPref, out_file_path);
}

}  // namespace prefs
}  // namespace games
