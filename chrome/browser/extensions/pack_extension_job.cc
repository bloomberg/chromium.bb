// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/pack_extension_job.h"

#include "app/l10n_util.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "base/task.h"
#include "chrome/browser/extensions/extension_creator.h"
#include "chrome/common/chrome_constants.h"
#include "grit/generated_resources.h"

PackExtensionJob::PackExtensionJob(Client* client,
                                   const FilePath& root_directory,
                                   const FilePath& key_file)
    : client_(client), key_file_(key_file) {
  root_directory_ = root_directory.StripTrailingSeparators();
  CHECK(BrowserThread::GetCurrentThreadIdentifier(&client_thread_id_));
}

void PackExtensionJob::Start() {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &PackExtensionJob::RunOnFileThread));
}

void PackExtensionJob::ClearClient() {
  client_ = NULL;
}

PackExtensionJob::~PackExtensionJob() {}

void PackExtensionJob::RunOnFileThread() {
  crx_file_out_ = FilePath(root_directory_.value() +
                           chrome::kExtensionFileExtension);

  if (key_file_.empty())
    key_file_out_ = FilePath(root_directory_.value() +
                             chrome::kExtensionKeyFileExtension);

  // TODO(aa): Need to internationalize the errors that ExtensionCreator
  // returns. See bug 20734.
  ExtensionCreator creator;
  if (creator.Run(root_directory_, crx_file_out_, key_file_, key_file_out_)) {
    BrowserThread::PostTask(
        client_thread_id_, FROM_HERE,
        NewRunnableMethod(this,
                          &PackExtensionJob::ReportSuccessOnClientThread));
  } else {
    BrowserThread::PostTask(
        client_thread_id_, FROM_HERE,
        NewRunnableMethod(
            this, &PackExtensionJob::ReportFailureOnClientThread,
            creator.error_message()));
  }
}

void PackExtensionJob::ReportSuccessOnClientThread() {
  if (client_)
    client_->OnPackSuccess(crx_file_out_, key_file_out_);
}

void PackExtensionJob::ReportFailureOnClientThread(const std::string& error) {
  if (client_)
    client_->OnPackFailure(error);
}

// static
std::wstring PackExtensionJob::StandardSuccessMessage(const FilePath& crx_file,
                                                      const FilePath& key_file)
{
  // TODO(isherman): we should use string16 instead of wstring.
  // See crbug.com/23581 and crbug.com/24672
  std::wstring message;
  if (key_file.empty()) {
    return l10n_util::GetStringF(
        IDS_EXTENSION_PACK_DIALOG_SUCCESS_BODY_UPDATE,
        crx_file.ToWStringHack());
  } else {
    return l10n_util::GetStringF(
        IDS_EXTENSION_PACK_DIALOG_SUCCESS_BODY_NEW,
        crx_file.ToWStringHack(),
        key_file.ToWStringHack());
  }
}
