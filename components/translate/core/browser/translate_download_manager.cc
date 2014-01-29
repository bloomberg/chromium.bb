// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_download_manager.h"

#include "base/memory/singleton.h"

// static
TranslateDownloadManager* TranslateDownloadManager::GetInstance() {
  return Singleton<TranslateDownloadManager>::get();
}

TranslateDownloadManager::TranslateDownloadManager() {}

TranslateDownloadManager::~TranslateDownloadManager() {}
