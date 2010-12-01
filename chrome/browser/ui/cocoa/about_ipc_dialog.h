// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_ABOUT_IPC_DIALOG_H_
#define CHROME_BROWSER_UI_COCOA_ABOUT_IPC_DIALOG_H_
#pragma once

#include "ipc/ipc_message.h"

#if defined(IPC_MESSAGE_LOG_ENABLED)

namespace AboutIPCDialog {
// The dialog is a singleton. If the dialog is already opened, it won't do
// anything, so you can just blindly call this function all you want.
// RunDialog() is Called from chrome/browser/browser_about_handler.cc
// in response to an about:ipc URL.
void RunDialog();
};


#endif  /* IPC_MESSAGE_LOG_ENABLED */

#endif  /* CHROME_BROWSER_UI_COCOA_ABOUT_IPC_DIALOG_H_ */
