// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_IMPORTER_EXTERNAL_PROCESS_IMPORTER_BRIDGE_H_
#define CHROME_UTILITY_IMPORTER_EXTERNAL_PROCESS_IMPORTER_BRIDGE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/importer/importer_bridge.h"

class GURL;
struct ImportedBookmarkEntry;

namespace base {
class DictionaryValue;
class TaskRunner;
}

namespace importer {
#if defined(OS_WIN)
struct ImporterIE7PasswordInfo;
#endif
struct ImporterURLRow;
struct URLKeywordInfo;
}

namespace IPC {
class Message;
class Sender;
}

// When the importer is run in an external process, the bridge is effectively
// split in half by the IPC infrastructure.  The external bridge receives data
// and notifications from the importer, and sends it across IPC.  The
// internal bridge gathers the data from the IPC host and writes it to the
// profile.
class ExternalProcessImporterBridge : public ImporterBridge {
 public:
  ExternalProcessImporterBridge(
      const base::DictionaryValue& localized_strings,
      IPC::Sender* sender,
      base::TaskRunner* task_runner);

  // Begin ImporterBridge implementation:
  virtual void AddBookmarks(const std::vector<ImportedBookmarkEntry>& bookmarks,
                            const base::string16& first_folder_name) OVERRIDE;

  virtual void AddHomePage(const GURL& home_page) OVERRIDE;

#if defined(OS_WIN)
  virtual void AddIE7PasswordInfo(
      const importer::ImporterIE7PasswordInfo& password_info) OVERRIDE;
#endif

  virtual void SetFavicons(
      const std::vector<ImportedFaviconUsage>& favicons) OVERRIDE;

  virtual void SetHistoryItems(const std::vector<ImporterURLRow>& rows,
                               importer::VisitSource visit_source) OVERRIDE;

  virtual void SetKeywords(
      const std::vector<importer::URLKeywordInfo>& url_keywords,
      bool unique_on_host_and_path) OVERRIDE;

  virtual void SetFirefoxSearchEnginesXMLData(
      const std::vector<std::string>& seach_engine_data) OVERRIDE;

  virtual void SetPasswordForm(
      const autofill::PasswordForm& form) OVERRIDE;

  virtual void NotifyStarted() OVERRIDE;
  virtual void NotifyItemStarted(importer::ImportItem item) OVERRIDE;
  virtual void NotifyItemEnded(importer::ImportItem item) OVERRIDE;
  virtual void NotifyEnded() OVERRIDE;

  virtual base::string16 GetLocalizedString(int message_id) OVERRIDE;
  // End ImporterBridge implementation.

 private:
  virtual ~ExternalProcessImporterBridge();

  void Send(IPC::Message* message);
  void SendInternal(IPC::Message* message);

  // Holds strings needed by the external importer because the resource
  // bundle isn't available to the external process.
  scoped_ptr<base::DictionaryValue> localized_strings_;

  IPC::Sender* sender_;
  scoped_refptr<base::TaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(ExternalProcessImporterBridge);
};

#endif  // CHROME_UTILITY_IMPORTER_EXTERNAL_PROCESS_IMPORTER_BRIDGE_H_
