// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_PROFILE_IMPORT_PROCESS_HOST_H_
#define CHROME_BROWSER_IMPORTER_PROFILE_IMPORT_PROCESS_HOST_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/importer/importer_data_types.h"
#include "chrome/browser/importer/profile_writer.h"
#include "content/browser/browser_child_process_host.h"
#include "content/browser/browser_thread.h"

class ProfileImportProcessClient;

namespace webkit_glue {
struct PasswordForm;
}

// Browser-side host to a profile import process.  This class lives only on
// the IO thread.  It passes messages back to the |thread_id_| thread through
// a client object.
class ProfileImportProcessHost : public BrowserChildProcessHost {
 public:
  // |import_process_client| implements callbacks which are triggered by
  // incoming IPC messages.  This client creates an interface between IPC
  // messages received by the ProfileImportProcessHost and the internal
  // importer_bridge.
  // |thread_id| gives the thread where the client lives. The
  // ProfileImportProcessHost spawns tasks on this thread for the client.
  ProfileImportProcessHost(ProfileImportProcessClient* import_process_client,
                           BrowserThread::ID thread_id);
  virtual ~ProfileImportProcessHost();

  // |source_profile|, |items|, and |import_to_bookmark_bar| are all needed by
  // the external importer process.
  bool StartProfileImportProcess(const importer::SourceProfile& source_profile,
                                 uint16 items,
                                 bool import_to_bookmark_bar);

  // Cancel the external import process.
  bool CancelProfileImportProcess();

  // Report that an item has been successfully imported.  We need to make
  // sure that all import messages have come across the wire before the
  // external import process shuts itself down.
  bool ReportImportItemFinished(importer::ImportItem item);

 protected:
  // This method is virtual to be overridden by tests.
  virtual FilePath GetProfileImportProcessCmd();

 private:
  // Launch the new process.
  bool StartProcess();

  // Called by the external importer process to send messages back to the
  // ImportProcessClient.
  virtual bool OnMessageReceived(const IPC::Message& message);

  // BrowserChildProcessHost:
  virtual void OnProcessCrashed(int exit_code) OVERRIDE;
  virtual bool CanShutdown() OVERRIDE;

  // Receives messages to be passed back to the importer host.
  scoped_refptr<ProfileImportProcessClient> import_process_client_;

  // The thread where the import_process_client_ lives.
  BrowserThread::ID thread_id_;

  DISALLOW_COPY_AND_ASSIGN(ProfileImportProcessHost);
};

#endif  // CHROME_BROWSER_IMPORTER_PROFILE_IMPORT_PROCESS_HOST_H_
