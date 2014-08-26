// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/page_action_controller.h"

#include "chrome/browser/extensions/extension_action_manager.h"
#include "content/public/browser/web_contents.h"

namespace extensions {

PageActionController::PageActionController(content::WebContents* web_contents)
    : web_contents_(web_contents),
      browser_context_(web_contents_->GetBrowserContext()) {
}

PageActionController::~PageActionController() {
}

ExtensionAction* PageActionController::GetActionForExtension(
    const Extension* extension) {
  return ExtensionActionManager::Get(browser_context_)->GetPageAction(
      *extension);
}

void PageActionController::OnNavigated() {
  // Clearing extension action values is handled in TabHelper, so nothing to
  // do here.
}

}  // namespace extensions
