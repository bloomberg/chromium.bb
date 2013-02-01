// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_MODAL_DIALOGS_JAVASCRIPT_DIALOG_MANAGER_H_
#define CHROME_BROWSER_UI_APP_MODAL_DIALOGS_JAVASCRIPT_DIALOG_MANAGER_H_

namespace content {
class JavaScriptDialogManager;
}

namespace extensions {
class ExtensionHost;
}

// Returns a JavaScriptDialogManager that creates real dialogs.
// It returns a Singleton instance of JavaScriptDialogManager,
// which should not be deleted.
content::JavaScriptDialogManager* GetJavaScriptDialogManagerInstance();

// Creates and returns a JavaScriptDialogManager owned by |extension_host|.
// This is not the Singleton instance, so the caller must delete it.
content::JavaScriptDialogManager* CreateJavaScriptDialogManagerInstance(
    extensions::ExtensionHost* extension_host);

#endif  // CHROME_BROWSER_UI_APP_MODAL_DIALOGS_JAVASCRIPT_DIALOG_MANAGER_H_
