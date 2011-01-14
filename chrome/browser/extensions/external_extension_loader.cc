// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_extension_loader.h"

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/extensions/external_extension_provider_impl.h"

void ExternalExtensionLoader::Init(
    ExternalExtensionProviderImpl* owner) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  owner_ = owner;
}

void ExternalExtensionLoader::OwnerShutdown() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  owner_ = NULL;
}

void ExternalExtensionLoader::LoadFinished() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  running_ = false;
  if (owner_) {
    owner_->SetPrefs(prefs_.release());
  }
}
