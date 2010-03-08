// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/pack_extension_job.h"

#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "base/task.h"
#include "chrome/browser/extensions/extension_creator.h"

PackExtensionJob::PackExtensionJob(Client* client,
                                   const FilePath& root_directory,
                                   const FilePath& key_file)
    : client_(client), root_directory_(root_directory), key_file_(key_file) {
  CHECK(ChromeThread::GetCurrentThreadIdentifier(&client_thread_id_));
}

void PackExtensionJob::Start() {
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &PackExtensionJob::RunOnFileThread));
}

void PackExtensionJob::ClearClient() {
  client_ = NULL;
}

void PackExtensionJob::RunOnFileThread() {
  crx_file_out_ = root_directory_.ReplaceExtension(FILE_PATH_LITERAL("crx"));

  if (key_file_.empty())
    key_file_out_ = root_directory_.ReplaceExtension(FILE_PATH_LITERAL("pem"));

  // TODO(aa): Need to internationalize the errors that ExtensionCreator
  // returns. See bug 20734.
  ExtensionCreator creator;
  if (creator.Run(root_directory_, crx_file_out_, key_file_, key_file_out_)) {
    ChromeThread::PostTask(
        client_thread_id_, FROM_HERE,
        NewRunnableMethod(this, &PackExtensionJob::ReportSuccessOnUIThread));
  } else {
    ChromeThread::PostTask(
        client_thread_id_, FROM_HERE,
        NewRunnableMethod(
            this, &PackExtensionJob::ReportFailureOnUIThread,
            creator.error_message()));
  }
}

void PackExtensionJob::ReportSuccessOnUIThread() {
  if (client_)
    client_->OnPackSuccess(crx_file_out_, key_file_out_);
}

void PackExtensionJob::ReportFailureOnUIThread(const std::string& error) {
  if (client_)
    client_->OnPackFailure(UTF8ToWide(error));
}
