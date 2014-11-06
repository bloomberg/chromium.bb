// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_APP_MODAL_DIALOGS_JAVASCRIPT_DIALOG_MANAGER_H_
#define COMPONENTS_APP_MODAL_DIALOGS_JAVASCRIPT_DIALOG_MANAGER_H_

#include "base/memory/scoped_ptr.h"

namespace content {
class JavaScriptDialogManager;
}

class JavaScriptDialogExtensionsClient;
class JavaScriptNativeDialogFactory;

// Returns a JavaScriptDialogManager that creates real dialogs.
// It returns a Singleton instance of JavaScriptDialogManager,
// which should not be deleted.
content::JavaScriptDialogManager* GetJavaScriptDialogManagerInstance();

// Sets the JavaScriptNativeDialogFactory used to create platform specific
// dialog window implementation.
void SetJavaScriptNativeDialogFactory(
    scoped_ptr<JavaScriptNativeDialogFactory> factory);

// JavaScript dialog may be opened by an extensions/app, thus needs
// access to extensions functionality. This sets a client interface to
// access //extensions.
void SetJavaScriptDialogExtensionsClient(
    scoped_ptr<JavaScriptDialogExtensionsClient> client);

#endif  // COMPONENTS_APP_MODAL_DIALOGS_JAVASCRIPT_DIALOG_MANAGER_H_
