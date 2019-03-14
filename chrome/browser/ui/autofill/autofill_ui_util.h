// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_UI_UTIL_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_UI_UTIL_H_

#include "content/public/browser/web_contents.h"

namespace autofill {

// TODO(crbug.com/932818): Clean this up and move the logic to
// ToolbarPageActionContainerView once the status chip is fully
// launched.

// Update the state of local card migration icon view.
void UpdateLocalCardMigrationIcon(content::WebContents* web_contents);

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_UI_UTIL_H_
