// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_PROFILE_IMPORT_HANDLER_H_
#define CHROME_UTILITY_PROFILE_IMPORT_HANDLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/utility/utility_message_handler.h"

class ExternalProcessImporterBridge;
class Importer;

namespace base {
class DictionaryValue;
class Thread;
}

namespace importer {
struct SourceProfile;
}

// Dispatches IPCs for out of process profile import.
class ProfileImportHandler : public UtilityMessageHandler {
 public:
  ProfileImportHandler();
  virtual ~ProfileImportHandler();

  // IPC::Listener:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  void OnImportStart(
      const importer::SourceProfile& source_profile,
      uint16 items,
      const base::DictionaryValue& localized_strings);
  void OnImportCancel();
  void OnImportItemFinished(uint16 item);

  // The following are used with out of process profile import:
  void ImporterCleanup();

  // Thread that importer runs on, while ProfileImportThread handles messages
  // from the browser process.
  scoped_ptr<base::Thread> import_thread_;

  // Bridge object is passed to importer, so that it can send IPC calls
  // directly back to the ProfileImportProcessHost.
  scoped_refptr<ExternalProcessImporterBridge> bridge_;

  // A bitmask of importer::ImportItem.
  uint16 items_to_import_;

  // Importer of the appropriate type (Firefox, Safari, IE, etc.)
  scoped_refptr<Importer> importer_;
};

#endif  // CHROME_UTILITY_PROFILE_IMPORT_HANDLER_H_
