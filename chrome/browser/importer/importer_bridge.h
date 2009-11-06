// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_IMPORTER_BRIDGE_H_
#define CHROME_BROWSER_IMPORTER_IMPORTER_BRIDGE_H_

#include "build/build_config.h"

#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"

#include "chrome/browser/importer/importer.h"
// TODO: remove this, see friend declaration in ImporterBridge.
#include "chrome/browser/importer/toolbar_importer.h"

class ImporterBridge : public base::RefCountedThreadSafe<ImporterBridge> {
 public:
  ImporterBridge(ProfileWriter* writer,
                 ImporterHost* host)
      : writer_(writer),
      host_(host) {
  }

  virtual void AddBookmarkEntries(
      const std::vector<ProfileWriter::BookmarkEntry>& bookmarks,
      const std::wstring& first_folder_name,
      int options) = 0;
  virtual void AddHomePage(const GURL &home_page) = 0;

#if defined(OS_WIN)
  virtual void AddIE7PasswordInfo(const IE7PasswordInfo password_info) = 0;
#endif

  virtual void SetFavIcons(
      const std::vector<history::ImportedFavIconUsage>& fav_icons) = 0;
  virtual void SetHistoryItems(const std::vector<history::URLRow> &rows) = 0;
  virtual void SetKeywords(const std::vector<TemplateURL*> &template_urls,
                           int default_keyword_index,
                           bool unique_on_host_and_path) = 0;
  virtual void SetPasswordForm(const webkit_glue::PasswordForm& form) = 0;

  // Notifies the coordinator that the collection of data for the specified
  // item has begun.
  virtual void NotifyItemStarted(ImportItem item) = 0;

  // Notifies the coordinator that the collection of data for the specified
  // item has completed.
  virtual void NotifyItemEnded(ImportItem item) = 0;

  // Notifies the coordinator that the import operation has begun.
  virtual void NotifyStarted() = 0;

  // Notifies the coordinator that the entire import operation has completed.
  virtual void NotifyEnded() = 0;

 protected:
  friend class base::RefCountedThreadSafe<ImporterBridge>;
  // TODO: In order to run Toolbar5Importer OOP we need to cut this
  // connection, but as an interim step we allow Toolbar5Import to break
  // the abstraction here and assume import is in-process.
  friend class Toolbar5Importer;

  virtual ~ImporterBridge() {}

  ProfileWriter* writer_;
  ImporterHost* host_;

  DISALLOW_COPY_AND_ASSIGN(ImporterBridge);
};

class InProcessImporterBridge : public ImporterBridge {
 public:
  InProcessImporterBridge(ProfileWriter* writer,
                          ImporterHost* host);

  // Methods inherited from ImporterBridge.
  virtual void AddBookmarkEntries(
      const std::vector<ProfileWriter::BookmarkEntry>& bookmarks,
      const std::wstring& first_folder_name,
      int options);
  virtual void AddHomePage(const GURL &home_page);

#if defined(OS_WIN)
  virtual void AddIE7PasswordInfo(const IE7PasswordInfo password_info);
#endif

  virtual void SetFavIcons(
    const std::vector<history::ImportedFavIconUsage>& fav_icons);
  virtual void SetHistoryItems(const std::vector<history::URLRow> &rows);
  virtual void SetKeywords(const std::vector<TemplateURL*>& template_urls,
                           int default_keyword_index,
                           bool unique_on_host_and_path);
  virtual void SetPasswordForm(const webkit_glue::PasswordForm& form);

  virtual void NotifyItemStarted(ImportItem item);
  virtual void NotifyItemEnded(ImportItem item);
  virtual void NotifyStarted();
  virtual void NotifyEnded();

 private:
  ~InProcessImporterBridge() {}

  DISALLOW_COPY_AND_ASSIGN(InProcessImporterBridge);
};

#endif  // CHROME_BROWSER_IMPORTER_IMPORTER_BRIDGE_H_
