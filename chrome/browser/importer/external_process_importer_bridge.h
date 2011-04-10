// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_EXTERNAL_PROCESS_IMPORTER_BRIDGE_H_
#define CHROME_BROWSER_IMPORTER_EXTERNAL_PROCESS_IMPORTER_BRIDGE_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/importer/importer_bridge.h"
#include "chrome/browser/importer/profile_writer.h"

class DictionaryValue;
class GURL;
class ProfileImportThread;

// When the importer is run in an external process, the bridge is effectively
// split in half by the IPC infrastructure.  The external bridge receives data
// and notifications from the importer, and sends it across IPC.  The
// internal bridge gathers the data from the IPC host and writes it to the
// profile.
class ExternalProcessImporterBridge : public ImporterBridge {
 public:
  ExternalProcessImporterBridge(ProfileImportThread* profile_import_thread,
                                const DictionaryValue& localized_strings);

  // Begin ImporterBridge implementation:
  virtual void AddBookmarkEntries(
      const std::vector<ProfileWriter::BookmarkEntry>& bookmarks,
      const string16& first_folder_name,
      int options) OVERRIDE;

  virtual void AddHomePage(const GURL& home_page) OVERRIDE;

#if defined(OS_WIN)
  virtual void AddIE7PasswordInfo(
      const IE7PasswordInfo& password_info) OVERRIDE;
#endif

  virtual void SetFavicons(
      const std::vector<history::ImportedFaviconUsage>& favicons) OVERRIDE;

  virtual void SetHistoryItems(const std::vector<history::URLRow>& rows,
                               history::VisitSource visit_source) OVERRIDE;

  virtual void SetKeywords(const std::vector<TemplateURL*>& template_urls,
                           int default_keyword_index,
                           bool unique_on_host_and_path) OVERRIDE;

  virtual void SetPasswordForm(const webkit_glue::PasswordForm& form) OVERRIDE;

  virtual void NotifyStarted() OVERRIDE;
  virtual void NotifyItemStarted(importer::ImportItem item) OVERRIDE;
  virtual void NotifyItemEnded(importer::ImportItem item) OVERRIDE;
  virtual void NotifyEnded() OVERRIDE;

  virtual string16 GetLocalizedString(int message_id) OVERRIDE;
  // End ImporterBridge implementation.

 private:
  virtual ~ExternalProcessImporterBridge();

  // Call back to send data and messages across IPC.
  ProfileImportThread* const profile_import_thread_;

  // Holds strings needed by the external importer because the resource
  // bundle isn't available to the external process.
  scoped_ptr<DictionaryValue> localized_strings_;

  DISALLOW_COPY_AND_ASSIGN(ExternalProcessImporterBridge);
};

#endif  // CHROME_BROWSER_IMPORTER_EXTERNAL_PROCESS_IMPORTER_BRIDGE_H_
