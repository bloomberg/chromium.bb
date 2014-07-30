// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "static_browser_cld_data_provider.h"

#include "base/logging.h"
#include "content/public/browser/web_contents.h"
#include "ipc/ipc_message.h"

namespace translate {

// Implementation of the static factory method from BrowserCldDataProvider,
// hooking up this specific implementation for all of Chromium.
BrowserCldDataProvider* CreateBrowserCldDataProviderFor(
    content::WebContents* web_contents) {
  // This log line is to help with determining which kind of provider has been
  // configured. See also: chrome://translate-internals
  VLOG(1) << "Creating StaticBrowserCldDataProvider";
  return new StaticBrowserCldDataProvider();
}

void SetCldDataFilePath(const base::FilePath& path) {
  LOG(WARNING) << "Not supported: SetCldDataFilePath";
  return;
}

base::FilePath GetCldDataFilePath() {
  return base::FilePath();  // empty path
}

void ConfigureBrowserCldDataProvider(const void* config) {
  // No-op: data is statically linked
}

StaticBrowserCldDataProvider::StaticBrowserCldDataProvider() {
}

StaticBrowserCldDataProvider::~StaticBrowserCldDataProvider() {
}

bool StaticBrowserCldDataProvider::OnMessageReceived(
    const IPC::Message& message) {
  // No-op: data is statically linked
  return false;
}

void StaticBrowserCldDataProvider::OnCldDataRequest() {
  // No-op: data is statically linked
}

void StaticBrowserCldDataProvider::SendCldDataResponse() {
  // No-op: data is statically linked
}

}  // namespace translate
