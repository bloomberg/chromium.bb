// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/pack_extension_job.h"

#include "base/message_loop.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/task.h"
#include "chrome/browser/extensions/extension_creator.h"
#include "chrome/common/chrome_constants.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

PackExtensionJob::PackExtensionJob(Client* client,
                                   const FilePath& root_directory,
                                   const FilePath& key_file)
    : client_(client), key_file_(key_file), asynchronous_(true) {
  root_directory_ = root_directory.StripTrailingSeparators();
  CHECK(BrowserThread::GetCurrentThreadIdentifier(&client_thread_id_));
}

void PackExtensionJob::Start() {
  if (asynchronous_) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableMethod(this, &PackExtensionJob::Run));
  } else {
    Run();
  }
}

void PackExtensionJob::ClearClient() {
  client_ = NULL;
}

PackExtensionJob::~PackExtensionJob() {}

void PackExtensionJob::Run() {
  crx_file_out_ = FilePath(root_directory_.value() +
                           chrome::kExtensionFileExtension);

  if (key_file_.empty())
    key_file_out_ = FilePath(root_directory_.value() +
                             chrome::kExtensionKeyFileExtension);

  // TODO(aa): Need to internationalize the errors that ExtensionCreator
  // returns. See bug 20734.
  ExtensionCreator creator;
  if (creator.Run(root_directory_, crx_file_out_, key_file_, key_file_out_)) {
    if (asynchronous_) {
      BrowserThread::PostTask(
          client_thread_id_, FROM_HERE,
          NewRunnableMethod(this,
                            &PackExtensionJob::ReportSuccessOnClientThread));
    } else {
      ReportSuccessOnClientThread();
    }
  } else {
    if (asynchronous_) {
      BrowserThread::PostTask(
          client_thread_id_, FROM_HERE,
          NewRunnableMethod(
              this, &PackExtensionJob::ReportFailureOnClientThread,
              creator.error_message()));
    } else {
      ReportFailureOnClientThread(creator.error_message());
    }
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
string16 PackExtensionJob::StandardSuccessMessage(const FilePath& crx_file,
                                                  const FilePath& key_file) {
  string16 crx_file_string = WideToUTF16(crx_file.ToWStringHack());
  string16 key_file_string = WideToUTF16(key_file.ToWStringHack());
  if (key_file_string.empty()) {
    return l10n_util::GetStringFUTF16(
        IDS_EXTENSION_PACK_DIALOG_SUCCESS_BODY_UPDATE,
        crx_file_string);
  } else {
    return l10n_util::GetStringFUTF16(
        IDS_EXTENSION_PACK_DIALOG_SUCCESS_BODY_NEW,
        crx_file_string,
        key_file_string);
  }
}
