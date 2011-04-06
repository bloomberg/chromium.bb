// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_EXTERNAL_PROCESS_IMPORTER_HOST_H_
#define CHROME_BROWSER_IMPORTER_EXTERNAL_PROCESS_IMPORTER_HOST_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/importer/importer_host.h"

class ExternalProcessImporterClient;
class Profile;
class ProfileWriter;

namespace importer {
struct SourceProfile;
}

// This class manages the import process. It creates the in-process half of the
// importer bridge and the external process importer client.
class ExternalProcessImporterHost : public ImporterHost {
 public:
  ExternalProcessImporterHost();

  // ImporterHost:
  virtual void Cancel() OVERRIDE;

 private:
  // ImporterHost:
  virtual void StartImportSettings(
      const importer::SourceProfile& source_profile,
      Profile* target_profile,
      uint16 items,
      ProfileWriter* writer,
      bool first_run) OVERRIDE;
  virtual void InvokeTaskIfDone() OVERRIDE;
  virtual void Loaded(BookmarkModel* model) OVERRIDE;

  // Used to pass notifications from the browser side to the external process.
  ExternalProcessImporterClient* client_;

  // Information about a profile needed for importing.
  const importer::SourceProfile* source_profile_;

  // Bitmask of items to be imported (see importer::ImportItem enum).
  uint16 items_;

  // Whether to import bookmarks to the bookmark bar.
  bool import_to_bookmark_bar_;

  // True if the import process has been cancelled.
  bool cancelled_;

  // True if the import process has been launched. This prevents race
  // conditions on import cancel.
  bool import_process_launched_;

  DISALLOW_COPY_AND_ASSIGN(ExternalProcessImporterHost);
};

#endif  // CHROME_BROWSER_IMPORTER_EXTERNAL_PROCESS_IMPORTER_HOST_H_
