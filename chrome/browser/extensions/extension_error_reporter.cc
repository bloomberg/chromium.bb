// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_error_reporter.h"

#include "build/build_config.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/simple_message_box.h"

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
    : ui_loop_(MessageLoop::current()),
      enable_noisy_errors_(enable_noisy_errors) {
}

ExtensionErrorReporter::~ExtensionErrorReporter() {}

void ExtensionErrorReporter::ReportError(const string16& message,
                                         bool be_noisy) {
  // NOTE: There won't be a ui_loop_ in the unit test environment.
  if (ui_loop_ && MessageLoop::current() != ui_loop_) {
    // base::Unretained is okay since the ExtensionErrorReporter is a singleton
    // that lives until the end of the process.
    ui_loop_->PostTask(FROM_HERE,
        base::Bind(&ExtensionErrorReporter::ReportError,
                   base::Unretained(this),
                   message,
                   be_noisy));
    return;
  }

  errors_.push_back(message);

  // TODO(aa): Print the error message out somewhere better. I think we are
  // going to need some sort of 'extension inspector'.
  LOG(ERROR) << "Extension error: " << message;

  if (enable_noisy_errors_ && be_noisy) {
    browser::ShowMessageBox(NULL, ASCIIToUTF16("Extension error"), message,
                            browser::MESSAGE_BOX_TYPE_WARNING);
  }
}

const std::vector<string16>* ExtensionErrorReporter::GetErrors() {
  return &errors_;
}

void ExtensionErrorReporter::ClearErrors() {
  errors_.clear();
}
