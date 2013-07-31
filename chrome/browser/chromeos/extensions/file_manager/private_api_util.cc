// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/private_api_util.h"

#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "content/public/browser/web_contents.h"

using google_apis::InstalledApp;

namespace file_manager {

int32 GetTabId(ExtensionFunctionDispatcher* dispatcher) {
  if (!dispatcher) {
    LOG(WARNING) << "No dispatcher";
    return 0;
  }
  if (!dispatcher->delegate()) {
    LOG(WARNING) << "No delegate";
    return 0;
  }
  content::WebContents* web_contents =
      dispatcher->delegate()->GetAssociatedWebContents();
  if (!web_contents) {
    LOG(WARNING) << "No associated tab contents";
    return 0;
  }
  return ExtensionTabUtil::GetTabId(web_contents);
}

GURL FindPreferredIcon(const InstalledApp::IconList& icons,
                       int preferred_size) {
  GURL result;
  if (icons.empty())
    return result;
  result = icons.rbegin()->second;
  for (InstalledApp::IconList::const_reverse_iterator iter = icons.rbegin();
       iter != icons.rend() && iter->first >= preferred_size; ++iter) {
    result = iter->second;
  }
  return result;
}

}  // namespace file_manager
