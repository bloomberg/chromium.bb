// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_error_reporter.h"

#include "build/build_config.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/platform_util.h"

// No AddRef required when using ExtensionErrorReporter with RunnableMethod.
// This is okay since the ExtensionErrorReporter is a singleton that lives until
// the end of the process.
DISABLE_RUNNABLE_METHOD_REFCOUNT(ExtensionErrorReporter);

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

void ExtensionErrorReporter::ReportError(const std::string& message,
                                         bool be_noisy) {
  // NOTE: There won't be a ui_loop_ in the unit test environment.
  if (ui_loop_ && MessageLoop::current() != ui_loop_) {
    ui_loop_->PostTask(FROM_HERE,
        NewRunnableMethod(this, &ExtensionErrorReporter::ReportError, message,
                          be_noisy));
    return;
  }

  errors_.push_back(message);

  // TODO(aa): Print the error message out somewhere better. I think we are
  // going to need some sort of 'extension inspector'.
  LOG(ERROR) << "Extension error: " << message;

  if (enable_noisy_errors_ && be_noisy) {
    platform_util::SimpleErrorBox(NULL,
                                  UTF8ToUTF16("Extension error"),
                                  UTF8ToUTF16(message));
  }
}

const std::vector<std::string>* ExtensionErrorReporter::GetErrors() {
  return &errors_;
}

void ExtensionErrorReporter::ClearErrors() {
  errors_.clear();
}
