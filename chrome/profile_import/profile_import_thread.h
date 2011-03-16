// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PROFILE_IMPORT_PROFILE_IMPORT_THREAD_H_
#define CHROME_PROFILE_IMPORT_PROFILE_IMPORT_THREAD_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/threading/thread.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/importer/importer_data_types.h"
#include "chrome/browser/importer/profile_writer.h"
#include "content/common/child_thread.h"
#include "webkit/glue/password_form.h"

class DictionaryValue;
class ExternalProcessImporterBridge;
class Importer;
class InProcessImporterBridge;

// This class represents the background thread which communicates with the
// importer work thread in the importer process.
class ProfileImportThread : public ChildThread {
 public:
  ProfileImportThread();
  virtual ~ProfileImportThread();

  // Returns the one profile import thread.
  static ProfileImportThread* current() {
    return static_cast<ProfileImportThread*>(ChildThread::current());
  }

  // Bridging methods, called from importer_bridge tasks posted here.
  void NotifyItemStarted(importer::ImportItem item);
  void NotifyItemEnded(importer::ImportItem item);
  void NotifyStarted();
  void NotifyEnded();

  // Bridging methods that move data back across the process boundary.
  void NotifyHistoryImportReady(const std::vector<history::URLRow> &rows,
                                history::VisitSource visit_source);
  void NotifyHomePageImportReady(const GURL& home_page);
  void NotifyBookmarksImportReady(
      const std::vector<ProfileWriter::BookmarkEntry>& bookmarks,
          const std::wstring& first_folder_name, int options);
  void NotifyFaviconsImportReady(
      const std::vector<history::ImportedFaviconUsage>& favicons);
  void NotifyPasswordFormReady(const webkit_glue::PasswordForm& form);
  void NotifyKeywordsReady(const std::vector<TemplateURL*>& template_urls,
      int default_keyword_index, bool unique_on_host_and_path);

 private:
  // IPC messages
  virtual bool OnControlMessageReceived(const IPC::Message& msg);

  // Creates the importer and launches it in a new thread. Import is run on
  // a separate thread so that this thread can receive messages from the
  // main process (especially cancel requests) while the worker thread handles
  // the actual import.
  void OnImportStart(
      const importer::ProfileInfo& profile_info,
      int items,
      const DictionaryValue& localized_strings,
      bool import_to_bookmark_bar);

  // Calls cleanup to stop the import operation.
  void OnImportCancel();

  // Called from the main process to notify that an item has been received
  // from the import process.
  void OnImportItemFinished(uint16 item);

  // Release the process and ourselves.
  void Cleanup();

  // Thread that importer runs on, while ProfileImportThread handles messages
  // from the browser process.
  scoped_ptr<base::Thread> import_thread_;

  // Bridge object is passed to importer, so that it can send IPC calls
  // directly back to the ProfileImportProcessHost.
  scoped_refptr<ExternalProcessImporterBridge> bridge_;

  // importer::ProfileType enum from importer_list, stored in ProfileInfo
  // struct in importer.
  int browser_type_;

  // A mask of importer::ImportItems.
  uint16 items_to_import_;

  // Importer of the appropriate type (Firefox, Safari, IE, etc.)
  scoped_refptr<Importer> importer_;

  DISALLOW_COPY_AND_ASSIGN(ProfileImportThread);
};

#endif  // CHROME_PROFILE_IMPORT_PROFILE_IMPORT_THREAD_H_
