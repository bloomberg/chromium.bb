// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_PREFS_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_PREFS_H_

#include <set>

#include "base/files/file_path.h"
#include "base/prefs/pref_member.h"

class PrefService;
class Profile;

namespace content {
class BrowserContext;
class DownloadManager;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// Stores all download-related preferences.
class DownloadPrefs {
 public:
  explicit DownloadPrefs(Profile* profile);
  ~DownloadPrefs();

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns the default download directory.
  static const base::FilePath& GetDefaultDownloadDirectory();

  // Returns the default download directory for the current profile.
  base::FilePath GetDefaultDownloadDirectoryForProfile() const;

  // Returns the DownloadPrefs corresponding to the given DownloadManager
  // or BrowserContext.
  static DownloadPrefs* FromDownloadManager(
      content::DownloadManager* download_manager);
  static DownloadPrefs* FromBrowserContext(
      content::BrowserContext* browser_context);

  base::FilePath DownloadPath() const;
  void SetDownloadPath(const base::FilePath& path);
  base::FilePath SaveFilePath() const;
  void SetSaveFilePath(const base::FilePath& path);
  int save_file_type() const { return *save_file_type_; }
  void SetSaveFileType(int type);

  // Returns true if the prompt_for_download preference has been set and the
  // download location is not managed (which means the user shouldn't be able
  // to choose another download location).
  bool PromptForDownload() const;

  // Returns true if the download path preference is managed.
  bool IsDownloadPathManaged() const;

  // Returns true if there is at least one file extension registered
  // for auto-open.
  bool IsAutoOpenUsed() const;

  // Returns true if |path| should be opened automatically based on
  // |path.Extension()|.
  bool IsAutoOpenEnabledBasedOnExtension(const base::FilePath& path) const;

  // Enables auto-open based on file extension. Returns true on success.
  // TODO(phajdan.jr): Add WARN_UNUSED_RESULT here.
  bool EnableAutoOpenBasedOnExtension(const base::FilePath& file_name);

  // Disables auto-open based on file extension.
  void DisableAutoOpenBasedOnExtension(const base::FilePath& file_name);

#if defined(OS_WIN)
  // Store the user preference to disk. If |should_open| is true, also disable
  // the built-in PDF plugin. If |should_open| is false, enable the PDF plugin.
  void SetShouldOpenPdfInAdobeReader(bool should_open);

  // Return whether the user prefers to open PDF downloads in Adobe Reader.
  bool ShouldOpenPdfInAdobeReader() const;
#endif

  void ResetAutoOpen();

 private:
  void SaveAutoOpenState();

  Profile* profile_;

  BooleanPrefMember prompt_for_download_;
  FilePathPrefMember download_path_;
  FilePathPrefMember save_file_path_;
  IntegerPrefMember save_file_type_;

  // Set of file extensions to open at download completion.
  struct AutoOpenCompareFunctor {
    bool operator()(const base::FilePath::StringType& a,
                    const base::FilePath::StringType& b) const;
  };
  typedef std::set<base::FilePath::StringType,
                   AutoOpenCompareFunctor> AutoOpenSet;
  AutoOpenSet auto_open_;

#if defined(OS_WIN)
  bool should_open_pdf_in_adobe_reader_;
#endif

  DISALLOW_COPY_AND_ASSIGN(DownloadPrefs);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_PREFS_H_
