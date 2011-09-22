// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_settings_ui_wrapper.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "chrome/browser/extensions/extension_settings.h"
#include "content/browser/browser_thread.h"

ExtensionSettingsUIWrapper::ExtensionSettingsUIWrapper(
    const FilePath& base_path)
    : core_(new ExtensionSettingsUIWrapper::Core()) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(
          &ExtensionSettingsUIWrapper::Core::InitOnFileThread,
          core_.get(),
          base_path));
}

void ExtensionSettingsUIWrapper::RunWithSettings(
    const SettingsCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(
          &ExtensionSettingsUIWrapper::Core::RunWithSettingsOnFileThread,
          core_.get(),
          callback));
}

ExtensionSettingsUIWrapper::~ExtensionSettingsUIWrapper() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

ExtensionSettingsUIWrapper::Core::Core()
    : extension_settings_(NULL) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void ExtensionSettingsUIWrapper::Core::InitOnFileThread(
    const FilePath& base_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!extension_settings_);
  extension_settings_ = new ExtensionSettings(base_path);
}

void ExtensionSettingsUIWrapper::Core::RunWithSettingsOnFileThread(
    const SettingsCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(extension_settings_);
  callback.Run(extension_settings_);
}

ExtensionSettingsUIWrapper::Core::~Core() {
  if (BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
    delete extension_settings_;
  } else if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::DeleteSoon(
        BrowserThread::FILE, FROM_HERE, extension_settings_);
  } else {
    NOTREACHED();
  }
}
