// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_PREFS_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_PREFS_H_

#include <set>

#include "base/file_path.h"
#include "base/prefs/public/pref_member.h"

class PrefRegistrySyncable;
class PrefService;
class Profile;

namespace content {
class BrowserContext;
class DownloadManager;
}

// Stores all download-related preferences.
class DownloadPrefs {
 public:
  explicit DownloadPrefs(Profile* profile);
  ~DownloadPrefs();

  static void RegisterUserPrefs(PrefRegistrySyncable* registry);

  // Returns the DownloadPrefs corresponding to the given DownloadManager
  // or BrowserContext.
  static DownloadPrefs* FromDownloadManager(
      content::DownloadManager* download_manager);
  static DownloadPrefs* FromBrowserContext(
      content::BrowserContext* browser_context);

  base::FilePath DownloadPath() const;
  int save_file_type() const { return *save_file_type_; }

  // Returns true if the prompt_for_download preference has been set and the
  // download location is not managed (which means the user shouldn't be able
  // to choose another download location).
  bool PromptForDownload() const;

  // Returns true if the download path preference is managed.
  bool IsDownloadPathManaged() const;

  // Returns true if there is at least one file extension registered
  // for auto-open.
  bool IsAutoOpenUsed() const;

  bool IsAutoOpenEnabledForExtension(
      const base::FilePath::StringType& extension) const;

  // Enables auto-open based on file extension. Returns true on success.
  // TODO(phajdan.jr): Add WARN_UNUSED_RESULT here.
  bool EnableAutoOpenBasedOnExtension(const base::FilePath& file_name);

  // Disables auto-open based on file extension.
  void DisableAutoOpenBasedOnExtension(const base::FilePath& file_name);

  void ResetAutoOpen();

 private:
  void SaveAutoOpenState();

  Profile* profile_;

  BooleanPrefMember prompt_for_download_;
  FilePathPrefMember download_path_;
  IntegerPrefMember save_file_type_;

  // Set of file extensions to open at download completion.
  struct AutoOpenCompareFunctor {
    bool operator()(const base::FilePath::StringType& a,
                    const base::FilePath::StringType& b) const;
  };
  typedef std::set<base::FilePath::StringType,
                   AutoOpenCompareFunctor> AutoOpenSet;
  AutoOpenSet auto_open_;

  DISALLOW_COPY_AND_ASSIGN(DownloadPrefs);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_PREFS_H_
