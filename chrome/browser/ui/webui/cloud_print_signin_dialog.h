// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CLOUD_PRINT_SIGNIN_DIALOG_H_
#define CHROME_BROWSER_UI_WEBUI_CLOUD_PRINT_SIGNIN_DIALOG_H_
#pragma once

class TabContents;

namespace cloud_print_signin_dialog {
// Creates a dialog for signing into cloud print.
// The dialog will close and refresh |parent_tab| when complete.
// Called on the UI thread. Even though this starts up a modal
// dialog, it will return immediately. The dialog is handled asynchronously.
void CreateCloudPrintSigninDialog(TabContents* parent_tab);
}

#endif  // CHROME_BROWSER_UI_WEBUI_CLOUD_PRINT_SIGNIN_DIALOG_H_

