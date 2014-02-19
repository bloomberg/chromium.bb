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
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(&ClearImageBurner));
    return;
  }

  chromeos::DBusThreadManager::Get()->
      GetImageBurnerClient()->
      ResetEventHandlers();
}

}  // namespace

void Operation::Write(const base::Closure& continuation) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  SetStage(image_writer_api::STAGE_WRITE);

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&Operation::StartWriteOnUIThread, this, continuation));

  AddCleanUpFunction(base::Bind(&ClearImageBurner));
}

void Operation::VerifyWrite(const base::Closure& continuation) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // No verification is available in Chrome OS currently.
  continuation.Run();
}

void Operation::StartWriteOnUIThread(const base::Closure& continuation) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ImageBurnerClient* burner =
      chromeos::DBusThreadManager::Get()->GetImageBurnerClient();

  burner->SetEventHandlers(
      base::Bind(&Operation::OnBurnFinished, this, continuation),
      base::Bind(&Operation::OnBurnProgress, this));

  burner->BurnImage(image_path_.value(),
                    device_path_.value(),
                    base::Bind(&Operation::OnBurnError, this));
}

void Operation::OnBurnFinished(const base::Closure& continuation,
                               const std::string& target_path,
                               bool success,
                               const std::string& error) {
  if (success) {
    SetProgress(kProgressComplete);
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE, continuation);
  } else {
    DLOG(ERROR) << "Error encountered while burning: " << error;
    Error(error::kChromeOSImageBurnerError);
  }
}

void Operation::OnBurnProgress(const std::string& target_path,
                               int64 num_bytes_burnt,
                               int64 total_size) {
  int progress = kProgressComplete * num_bytes_burnt / total_size;
  SetProgress(progress);
}

void Operation::OnBurnError() {
  Error(error::kChromeOSImageBurnerError);
}

}  // namespace image_writer
}  // namespace extensions
