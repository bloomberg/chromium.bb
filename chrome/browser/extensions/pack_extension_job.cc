// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/pack_extension_job.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/task.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/extensions/extension_creator.h"

PackExtensionJob::PackExtensionJob(Client* client,
                                   const FilePath& root_directory,
                                   const FilePath& key_file)
    : ui_loop_(MessageLoop::current()), client_(client),
      root_directory_(root_directory), key_file_(key_file) {
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
    ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
        &PackExtensionJob::ReportSuccessOnUIThread));
  } else {
    ui_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
        &PackExtensionJob::ReportFailureOnUIThread, creator.error_message()));
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
