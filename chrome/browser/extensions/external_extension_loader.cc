// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_extension_loader.h"

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/extensions/external_extension_provider_impl.h"
#include "content/browser/browser_thread.h"

ExternalExtensionLoader::ExternalExtensionLoader()
    : owner_(NULL),
      running_(false) {
}

void ExternalExtensionLoader::Init(
    ExternalExtensionProviderImpl* owner) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  owner_ = owner;
}

const FilePath ExternalExtensionLoader::GetBaseCrxFilePath() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // By default, relative paths are not supported.
  // Subclasses that wish to support them should override this method.
  return FilePath();
}

void ExternalExtensionLoader::OwnerShutdown() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  owner_ = NULL;
}

ExternalExtensionLoader::~ExternalExtensionLoader() {}

void ExternalExtensionLoader::LoadFinished() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  running_ = false;
  if (owner_) {
    owner_->SetPrefs(prefs_.release());
  }
}
