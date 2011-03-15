// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_IMPORTER_BRIDGE_H_
#define CHROME_BROWSER_IMPORTER_IMPORTER_BRIDGE_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/importer/importer_data_types.h"
// TODO: remove this, see friend declaration in ImporterBridge.
#include "chrome/browser/importer/toolbar_importer.h"

class ProfileImportThread;
class DictionaryValue;
class ImporterHost;

class ImporterBridge : public base::RefCountedThreadSafe<ImporterBridge> {
 public:
  ImporterBridge();

  virtual void AddBookmarkEntries(
      const std::vector<ProfileWriter::BookmarkEntry>& bookmarks,
      const std::wstring& first_folder_name,
      int options) = 0;
  virtual void AddHomePage(const GURL &home_page) = 0;

#if defined(OS_WIN)
  virtual void AddIE7PasswordInfo(const IE7PasswordInfo password_info) = 0;
#endif

  virtual void SetFavicons(
      const std::vector<history::ImportedFaviconUsage>& favicons) = 0;
  virtual void SetHistoryItems(const std::vector<history::URLRow> &rows,
                               history::VisitSource visit_source) = 0;
  virtual void SetKeywords(const std::vector<TemplateURL*> &template_urls,
                           int default_keyword_index,
                           bool unique_on_host_and_path) = 0;
  virtual void SetPasswordForm(const webkit_glue::PasswordForm& form) = 0;

  // Notifies the coordinator that the import operation has begun.
  virtual void NotifyStarted() = 0;

  // Notifies the coordinator that the collection of data for the specified
  // item has begun.
  virtual void NotifyItemStarted(importer::ImportItem item) = 0;

  // Notifies the coordinator that the collection of data for the specified
  // item has completed.
  virtual void NotifyItemEnded(importer::ImportItem item) = 0;

  // Notifies the coordinator that the entire import operation has completed.
  virtual void NotifyEnded() = 0;

  // For InProcessImporters this calls l10n_util. For ExternalProcessImporters
  // this calls the set of strings we've ported over to the external process.
  // It's good to avoid having to create a separate ResourceBundle for the
  // external import process, since the importer only needs a few strings.
  virtual std::wstring GetLocalizedString(int message_id) = 0;

 protected:
  friend class base::RefCountedThreadSafe<ImporterBridge>;
  // TODO: In order to run Toolbar5Importer OOP we need to cut this
  // connection, but as an interim step we allow Toolbar5Import to break
  // the abstraction here and assume import is in-process.
  friend class Toolbar5Importer;

  virtual ~ImporterBridge();

  DISALLOW_COPY_AND_ASSIGN(ImporterBridge);
};

class InProcessImporterBridge : public ImporterBridge {
 public:
  InProcessImporterBridge(ProfileWriter* writer,
                          ImporterHost* host);

  // Methods inherited from ImporterBridge.  On the internal side, these
  // methods launch tasks to write the data to the profile with the |writer_|.
  virtual void AddBookmarkEntries(
      const std::vector<ProfileWriter::BookmarkEntry>& bookmarks,
      const std::wstring& first_folder_name,
      int options);
  virtual void AddHomePage(const GURL &home_page);

#if defined(OS_WIN)
  virtual void AddIE7PasswordInfo(const IE7PasswordInfo password_info);
#endif

  virtual void SetFavicons(
      const std::vector<history::ImportedFaviconUsage>& favicons);
  virtual void SetHistoryItems(const std::vector<history::URLRow> &rows,
                               history::VisitSource visit_source);
  virtual void SetKeywords(const std::vector<TemplateURL*>& template_urls,
                           int default_keyword_index,
                           bool unique_on_host_and_path);
  virtual void SetPasswordForm(const webkit_glue::PasswordForm& form);

  virtual void NotifyItemStarted(importer::ImportItem item);
  virtual void NotifyItemEnded(importer::ImportItem item);
  virtual void NotifyStarted();
  virtual void NotifyEnded();
  virtual std::wstring GetLocalizedString(int message_id);

 private:
  virtual ~InProcessImporterBridge();

  ProfileWriter* const writer_;  // weak
  ImporterHost* const host_;  // weak

  DISALLOW_COPY_AND_ASSIGN(InProcessImporterBridge);
};

// When the importer is run in an external process, the bridge is effectively
// split in half by the IPC infrastructure.  The external bridge receives data
// and notifications from the importer, and sends it across IPC.  The
// internal bridge gathers the data from the IPC host and writes it to the
// profile.
class ExternalProcessImporterBridge : public ImporterBridge {
 public:
  ExternalProcessImporterBridge(ProfileImportThread* profile_import_thread,
                                const DictionaryValue& localized_strings);

  // Methods inherited from ImporterBridge.  On the external side, these
  // methods gather data and give it to a ProfileImportThread to pass back
  // to the browser process.
  virtual void AddBookmarkEntries(
      const std::vector<ProfileWriter::BookmarkEntry>& bookmarks,
      const std::wstring& first_folder_name, int options);
  virtual void AddHomePage(const GURL &home_page);

#if defined(OS_WIN)
  virtual void AddIE7PasswordInfo(const IE7PasswordInfo password_info);
#endif

  virtual void SetFavicons(
      const std::vector<history::ImportedFaviconUsage>& favicons);
  virtual void SetHistoryItems(const std::vector<history::URLRow> &rows,
                               history::VisitSource visit_source);
  virtual void SetKeywords(const std::vector<TemplateURL*>& template_urls,
                           int default_keyword_index,
                           bool unique_on_host_and_path);
  virtual void SetPasswordForm(const webkit_glue::PasswordForm& form);

  virtual void NotifyStarted();
  virtual void NotifyItemStarted(importer::ImportItem item);
  virtual void NotifyItemEnded(importer::ImportItem item);
  virtual void NotifyEnded();

  virtual std::wstring GetLocalizedString(int message_id);

 private:
  ~ExternalProcessImporterBridge();

  // Call back to send data and messages across IPC.
  ProfileImportThread* const profile_import_thread_;

  // Holds strings needed by the external importer because the resource
  // bundle isn't available to the external process.
  scoped_ptr<DictionaryValue> localized_strings_;

  DISALLOW_COPY_AND_ASSIGN(ExternalProcessImporterBridge);
};

#endif  // CHROME_BROWSER_IMPORTER_IMPORTER_BRIDGE_H_
