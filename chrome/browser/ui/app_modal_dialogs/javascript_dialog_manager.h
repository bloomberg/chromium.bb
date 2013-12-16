// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_MODAL_DIALOGS_JAVASCRIPT_DIALOG_MANAGER_H_
#define CHROME_BROWSER_UI_APP_MODAL_DIALOGS_JAVASCRIPT_DIALOG_MANAGER_H_

namespace content {
class JavaScriptDialogManager;
}

// Returns a JavaScriptDialogManager that creates real dialogs.
// It returns a Singleton instance of JavaScriptDialogManager,
// which should not be deleted.
content::JavaScriptDialogManager* GetJavaScriptDialogManagerInstance();

#endif  // CHROME_BROWSER_UI_APP_MODAL_DIALOGS_JAVASCRIPT_DIALOG_MANAGER_H_
