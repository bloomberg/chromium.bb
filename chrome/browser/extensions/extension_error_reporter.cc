// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_error_reporter.h"

#include "build/build_config.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "content/public/browser/notification_service.h"

ExtensionErrorReporter* ExtensionErrorReporter::instance_ = NULL;

// static
void ExtensionErrorReporter::Init(bool enable_noisy_errors) {
  if (!instance_) {
    instance_ = new ExtensionErrorReporter(enable_noisy_errors);
  }
}

// static
ExtensionErrorReporter* ExtensionErrorReporter::GetInstance() {
  CHECK(instance_) << "Init() was never called";
  return instance_;
}

ExtensionErrorReporter::ExtensionErrorReporter(bool enable_noisy_errors)
    : ui_loop_(base::MessageLoop::current()),
      enable_noisy_errors_(enable_noisy_errors) {
}

ExtensionErrorReporter::~ExtensionErrorReporter() {}

void ExtensionErrorReporter::ReportLoadError(
    const base::FilePath& extension_path,
    const std::string& error,
    Profile* profile,
    bool be_noisy) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_LOAD_ERROR,
      content::Source<Profile>(profile),
      content::Details<const std::string>(&error));

  std::string path_str = base::UTF16ToUTF8(extension_path.LossyDisplayName());
  base::string16 message = base::UTF8ToUTF16(
      base::StringPrintf("Could not load extension from '%s'. %s",
                         path_str.c_str(),
                         error.c_str()));
  ReportError(message, be_noisy);
  FOR_EACH_OBSERVER(Observer,
                    observers_,
                    OnLoadFailure(extension_path, error));
}

void ExtensionErrorReporter::ReportError(const base::string16& message,
                                         bool be_noisy) {
  // NOTE: There won't be a ui_loop_ in the unit test environment.
  if (ui_loop_) {
    CHECK(base::MessageLoop::current() == ui_loop_)
        << "ReportError can only be called from the UI thread.";
  }

  errors_.push_back(message);

  // TODO(aa): Print the error message out somewhere better. I think we are
  // going to need some sort of 'extension inspector'.
  LOG(WARNING) << "Extension error: " << message;

  if (enable_noisy_errors_ && be_noisy) {
    chrome::ShowMessageBox(NULL,
                           base::ASCIIToUTF16("Extension error"),
                           message,
                           chrome::MESSAGE_BOX_TYPE_WARNING);
  }
}

const std::vector<base::string16>* ExtensionErrorReporter::GetErrors() {
  return &errors_;
}

void ExtensionErrorReporter::ClearErrors() {
  errors_.clear();
}

void ExtensionErrorReporter::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ExtensionErrorReporter::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}
