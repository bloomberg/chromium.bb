// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/error_messages.h"
#include "chrome/browser/extensions/api/image_writer_private/operation.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/image_burner_client.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {
namespace image_writer {

using chromeos::ImageBurnerClient;
using content::BrowserThread;

namespace {

void ClearImageBurner() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  chromeos::DBusThreadManager::Get()->
      GetImageBurnerClient()->
      ResetEventHandlers();
}

void CleanUpImageBurner() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&ClearImageBurner));
}

}  // namespace

void Operation::WriteStart() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  SetStage(image_writer_api::STAGE_WRITE);

  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&Operation::StartWriteOnUIThread, this));

  AddCleanUpFunction(base::Bind(&CleanUpImageBurner));
}

void Operation::StartWriteOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DVLOG(1) << "Starting burn.";

  ImageBurnerClient* burner =
      chromeos::DBusThreadManager::Get()->GetImageBurnerClient();

  burner->SetEventHandlers(
      base::Bind(&Operation::OnBurnFinished, this),
      base::Bind(&Operation::OnBurnProgress, this));

  burner->BurnImage(image_path_.value(),
                    storage_unit_id_,
                    base::Bind(&Operation::OnBurnError, this));
}

void Operation::OnBurnFinished(const std::string& target_path,
                    bool success,
                    const std::string& error) {
  DVLOG(1) << "Burn finished: " << success;

  if (success) {
    SetProgress(kProgressComplete);
    Finish();
  } else {
    Error(error);
  }
}

void Operation::OnBurnProgress(const std::string& target_path,
                    int64 num_bytes_burnt,
                    int64 total_size) {
  int progress = kProgressComplete * num_bytes_burnt / total_size;
  SetProgress(progress);
}

void Operation::OnBurnError() {
  Error(error::kImageBurnerError);
}

}  // namespace image_writer
}  // namespace extensions
