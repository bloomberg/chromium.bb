// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/external_process_importer_host.h"

#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/importer/external_process_importer_client.h"
#include "chrome/browser/importer/importer_bridge.h"

ExternalProcessImporterHost::ExternalProcessImporterHost()
    : items_(0),
      import_to_bookmark_bar_(false),
      cancelled_(false),
      import_process_launched_(false) {
}

void ExternalProcessImporterHost::Cancel() {
  cancelled_ = true;
  if (import_process_launched_)
    client_->Cancel();
  NotifyImportEnded();  // Tells the observer that we're done, and releases us.
}

void ExternalProcessImporterHost::StartImportSettings(
    const importer::ProfileInfo& profile_info,
    Profile* target_profile,
    uint16 items,
    ProfileWriter* writer,
    bool first_run) {
  DCHECK(!profile_);
  profile_ = target_profile;
  writer_ = writer;
  profile_info_ = &profile_info;
  items_ = items;

  ImporterHost::AddRef();  // Balanced in ImporterHost::NotifyImportEnded.

  import_to_bookmark_bar_ = ShouldImportToBookmarkBar(first_run);
  CheckForFirefoxLock(profile_info, items, first_run);
  CheckForLoadedModels(items);

  InvokeTaskIfDone();
}

void ExternalProcessImporterHost::InvokeTaskIfDone() {
  if (waiting_for_bookmarkbar_model_ || !registrar_.IsEmpty() ||
      !is_source_readable_ || cancelled_)
    return;

  // The in-process half of the bridge which catches data from the IPC pipe
  // and feeds it to the ProfileWriter.  The external process half of the
  // bridge lives in the external process -- see ProfileImportThread.
  // The ExternalProcessImporterClient created in the next line owns this
  // bridge, and will delete it.
  InProcessImporterBridge* bridge =
      new InProcessImporterBridge(writer_.get(), this);
  client_ = new ExternalProcessImporterClient(this, *profile_info_, items_,
                                              bridge, import_to_bookmark_bar_);
  import_process_launched_ = true;
  client_->Start();
}

void ExternalProcessImporterHost::Loaded(BookmarkModel* model) {
  DCHECK(model->IsLoaded());
  model->RemoveObserver(this);
  waiting_for_bookmarkbar_model_ = false;
  installed_bookmark_observer_ = false;

  // Because the import process is running externally, the decision whether
  // to import to the bookmark bar must be stored here so that it can be
  // passed to the importer when the import task is invoked.
  import_to_bookmark_bar_ = (!model->HasBookmarks());
  InvokeTaskIfDone();
}
