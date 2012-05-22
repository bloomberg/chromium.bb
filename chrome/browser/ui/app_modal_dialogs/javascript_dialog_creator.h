// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_MODAL_DIALOGS_JAVASCRIPT_DIALOG_CREATOR_H_
#define CHROME_BROWSER_UI_APP_MODAL_DIALOGS_JAVASCRIPT_DIALOG_CREATOR_H_
#pragma once

namespace content {
class JavaScriptDialogCreator;
}

class ExtensionHost;

// Returns a JavaScriptDialogCreator that creates real dialogs.
// It returns a Singleton instance of JavaScriptDialogCreator,
// which should not be deleted.
content::JavaScriptDialogCreator* GetJavaScriptDialogCreatorInstance();

// Creates and returns a JavaScriptDialogCreator owned by |extension_host|.
// This is not the Singleton instance, so the caller must delete it.
content::JavaScriptDialogCreator* CreateJavaScriptDialogCreatorInstance(
    ExtensionHost* extension_host);

#endif  // CHROME_BROWSER_UI_APP_MODAL_DIALOGS_JAVASCRIPT_DIALOG_CREATOR_H_
