// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_settings_frontend.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "chrome/browser/extensions/extension_settings_backend.h"
#include "content/browser/browser_thread.h"

ExtensionSettingsFrontend::ExtensionSettingsFrontend(
    const FilePath& base_path)
    : core_(new ExtensionSettingsFrontend::Core()) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(
          &ExtensionSettingsFrontend::Core::InitOnFileThread,
          core_.get(),
          base_path));
}

void ExtensionSettingsFrontend::RunWithBackend(
    const BackendCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(
          &ExtensionSettingsFrontend::Core::RunWithBackendOnFileThread,
          core_.get(),
          callback));
}

ExtensionSettingsFrontend::~ExtensionSettingsFrontend() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

ExtensionSettingsFrontend::Core::Core() : backend_(NULL) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void ExtensionSettingsFrontend::Core::InitOnFileThread(
    const FilePath& base_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!backend_);
  backend_ = new ExtensionSettingsBackend(base_path);
}

void ExtensionSettingsFrontend::Core::RunWithBackendOnFileThread(
    const BackendCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(backend_);
  callback.Run(backend_);
}

ExtensionSettingsFrontend::Core::~Core() {
  if (BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
    delete backend_;
  } else if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::DeleteSoon(BrowserThread::FILE, FROM_HERE, backend_);
  } else {
    NOTREACHED();
  }
}
