// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_IMPORTER_BRIDGE_H_
#define CHROME_BROWSER_IMPORTER_IMPORTER_BRIDGE_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "build/build_config.h"
#include "chrome/browser/importer/importer_data_types.h"
#include "chrome/browser/importer/profile_writer.h"

// TODO: remove this, see friend declaration in ImporterBridge.
class Toolbar5Importer;

class ImporterBridge : public base::RefCountedThreadSafe<ImporterBridge> {
 public:
  ImporterBridge();

  virtual void AddBookmarkEntries(
      const std::vector<ProfileWriter::BookmarkEntry>& bookmarks,
      const string16& first_folder_name,
      int options) = 0;

  virtual void AddHomePage(const GURL& home_page) = 0;

#if defined(OS_WIN)
  virtual void AddIE7PasswordInfo(const IE7PasswordInfo& password_info) = 0;
#endif

  virtual void SetFavicons(
      const std::vector<history::ImportedFaviconUsage>& favicons) = 0;

  virtual void SetHistoryItems(const std::vector<history::URLRow>& rows,
                               history::VisitSource visit_source) = 0;

  virtual void SetKeywords(const std::vector<TemplateURL*>& template_urls,
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
  virtual string16 GetLocalizedString(int message_id) = 0;

 protected:
  friend class base::RefCountedThreadSafe<ImporterBridge>;
  // TODO: In order to run Toolbar5Importer OOP we need to cut this
  // connection, but as an interim step we allow Toolbar5Import to break
  // the abstraction here and assume import is in-process.
  friend class Toolbar5Importer;

  virtual ~ImporterBridge();

  DISALLOW_COPY_AND_ASSIGN(ImporterBridge);
};

#endif  // CHROME_BROWSER_IMPORTER_IMPORTER_BRIDGE_H_
