// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_PROFILE_IMPORT_HANDLER_H_
#define CHROME_UTILITY_PROFILE_IMPORT_HANDLER_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/common/importer/profile_import.mojom.h"
#include "chrome/utility/utility_message_handler.h"
#include "mojo/public/cpp/bindings/interface_request.h"

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
class ProfileImportHandler : public chrome::mojom::ProfileImport {
 public:
  ProfileImportHandler();
  ~ProfileImportHandler() override;

  static void Create(
      mojo::InterfaceRequest<chrome::mojom::ProfileImport> request);

 private:
  // chrome::mojom::ProfileImport:
  void StartImport(const importer::SourceProfile& source_profile,
                   uint16_t items,
                   const base::DictionaryValue& localized_strings,
                   chrome::mojom::ProfileImportObserverPtr observer) override;
  void CancelImport() override;
  void ReportImportItemFinished(importer::ImportItem item) override;

  // The following are used with out of process profile import:
  void ImporterCleanup();

  // Thread that importer runs on, while ProfileImportThread handles messages
  // from the browser process.
  std::unique_ptr<base::Thread> import_thread_;

  // Bridge object is passed to importer, so that it can send IPC calls
  // directly back to the ProfileImportProcessHost.
  scoped_refptr<ExternalProcessImporterBridge> bridge_;

  // A bitmask of importer::ImportItem.
  uint16_t items_to_import_;

  // Importer of the appropriate type (Firefox, Safari, IE, etc.)
  scoped_refptr<Importer> importer_;

  DISALLOW_COPY_AND_ASSIGN(ProfileImportHandler);
};

#endif  // CHROME_UTILITY_PROFILE_IMPORT_HANDLER_H_
