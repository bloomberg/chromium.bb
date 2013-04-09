// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/profile_import_handler.h"

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop_proxy.h"
#include "base/threading/thread.h"
#include "chrome/browser/importer/external_process_importer_bridge.h"
#include "chrome/browser/importer/importer.h"
#include "chrome/browser/importer/profile_import_process_messages.h"
#include "content/public/utility/utility_thread.h"

namespace chrome {

ProfileImportHandler::ProfileImportHandler() : items_to_import_(0) {}

ProfileImportHandler::~ProfileImportHandler() {}

bool ProfileImportHandler::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ProfileImportHandler, message)
    IPC_MESSAGE_HANDLER(ProfileImportProcessMsg_StartImport, OnImportStart)
    IPC_MESSAGE_HANDLER(ProfileImportProcessMsg_CancelImport, OnImportCancel)
    IPC_MESSAGE_HANDLER(ProfileImportProcessMsg_ReportImportItemFinished,
                        OnImportItemFinished)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ProfileImportHandler::OnImportStart(
    const importer::SourceProfile& source_profile,
    uint16 items,
    const base::DictionaryValue& localized_strings) {
  bridge_ = new ExternalProcessImporterBridge(
      localized_strings, content::UtilityThread::Get(),
      base::MessageLoopProxy::current());
  importer_ = importer::CreateImporterByType(source_profile.importer_type);
  if (!importer_) {
    Send(new ProfileImportProcessHostMsg_Import_Finished(false,
        "Importer could not be created."));
    return;
  }

  items_to_import_ = items;

  // Create worker thread in which importer runs.
  import_thread_.reset(new base::Thread("import_thread"));
  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  if (!import_thread_->StartWithOptions(options)) {
    NOTREACHED();
    ImporterCleanup();
  }
  import_thread_->message_loop()->PostTask(
      FROM_HERE, base::Bind(&Importer::StartImport, importer_.get(),
                            source_profile, items, bridge_));
}

void ProfileImportHandler::OnImportCancel() {
  ImporterCleanup();
}

void ProfileImportHandler::OnImportItemFinished(uint16 item) {
  items_to_import_ ^= item;  // Remove finished item from mask.
  // If we've finished with all items, notify the browser process.
  if (items_to_import_ == 0) {
    Send(new ProfileImportProcessHostMsg_Import_Finished(true, std::string()));
    ImporterCleanup();
  }
}

void ProfileImportHandler::ImporterCleanup() {
  importer_->Cancel();
  importer_ = NULL;
  bridge_ = NULL;
  import_thread_.reset();
  content::UtilityThread::Get()->ReleaseProcessIfNeeded();
}

// static
bool ProfileImportHandler::Send(IPC::Message* message) {
  return content::UtilityThread::Get()->Send(message);
}

}  // namespace chrome
