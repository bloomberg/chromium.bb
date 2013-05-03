// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/external_process_importer_host.h"

#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/importer/external_process_importer_client.h"
#include "chrome/browser/importer/importer_type.h"
#include "chrome/browser/importer/in_process_importer_bridge.h"

ExternalProcessImporterHost::ExternalProcessImporterHost()
    : client_(NULL),
      source_profile_(NULL),
      items_(0),
      cancelled_(false),
      import_process_launched_(false) {
}

void ExternalProcessImporterHost::Cancel() {
  cancelled_ = true;
  if (import_process_launched_)
    client_->Cancel();
  NotifyImportEnded();  // Tells the observer that we're done, and deletes us.
}

ExternalProcessImporterHost::~ExternalProcessImporterHost() {}

void ExternalProcessImporterHost::StartImportSettings(
    const importer::SourceProfile& source_profile,
    Profile* target_profile,
    uint16 items,
    ProfileWriter* writer) {
  // We really only support importing from one host at a time.
  DCHECK(!profile_);
  DCHECK(target_profile);

  profile_ = target_profile;
  writer_ = writer;
  source_profile_ = &source_profile;
  items_ = items;

  CheckForFirefoxLock(source_profile, items);
  CheckForLoadedModels(items);

  InvokeTaskIfDone();
}

void ExternalProcessImporterHost::InvokeTaskIfDone() {
  if (waiting_for_bookmarkbar_model_ || !registrar_.IsEmpty() ||
      !is_source_readable_ || cancelled_)
    return;

  // This is the in-process half of the bridge, which catches data from the IPC
  // pipe and feeds it to the ProfileWriter. The external process half of the
  // bridge lives in the external process (see ProfileImportThread).
  // The ExternalProcessImporterClient created in the next line owns the bridge,
  // and will delete it.
  InProcessImporterBridge* bridge =
      new InProcessImporterBridge(writer_.get(),
                                  weak_ptr_factory_.GetWeakPtr());
  client_ = new ExternalProcessImporterClient(this, *source_profile_, items_,
                                              bridge);
  import_process_launched_ = true;
  client_->Start();
}

void ExternalProcessImporterHost::Loaded(BookmarkModel* model,
                                         bool ids_reassigned) {
  DCHECK(model->IsLoaded());
  model->RemoveObserver(this);
  waiting_for_bookmarkbar_model_ = false;
  installed_bookmark_observer_ = false;

  InvokeTaskIfDone();
}
