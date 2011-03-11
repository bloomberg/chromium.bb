// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_DIALOGS_H_
#define CHROME_BROWSER_UI_BROWSER_DIALOGS_H_
#pragma once

#include "ipc/ipc_message.h"
#include "ui/gfx/native_widget_types.h"

class Browser;
class Extension;
class ImporterHost;
class HtmlDialogUIDelegate;
class Profile;
class SkBitmap;
class TabContents;

namespace browser {

#if defined(IPC_MESSAGE_LOG_ENABLED)

// The dialog is a singleton. If the dialog is already opened, it won't do
// anything, so you can just blindly call this function all you want.
// This is Called from chrome/browser/browser_about_handler.cc
void ShowAboutIPCDialog();

#endif  // IPC_MESSAGE_LOG_ENABLED

// Creates and shows an HTML dialog with the given delegate and profile.
// The window is automatically destroyed when it is closed.
// Returns the created window.
//
// Make sure to use the returned window only when you know it is safe
// to do so, i.e. before OnDialogClosed() is called on the delegate.
gfx::NativeWindow ShowHtmlDialog(gfx::NativeWindow parent, Profile* profile,
                                 HtmlDialogUIDelegate* delegate);

// This function is called by an ImporterHost, and displays the Firefox profile
// locked warning by creating a dialog.  On the closing of the dialog, the
// ImportHost receives a callback with the message either to skip the import,
// or to try again.
void ShowImportLockDialog(gfx::NativeWindow parent,
                          ImporterHost* importer_host);

// Creates the ExtensionInstalledBubble and schedules it to be shown once
// the extension has loaded. |extension| is the installed extension. |browser|
// is the browser window which will host the bubble. |icon| is the install
// icon of the extension.
void ShowExtensionInstalledBubble(const Extension* extension,
                                  Browser* browser,
                                  const SkBitmap& icon,
                                  Profile* profile);

// Shows or hide the hung renderer dialog for the given TabContents.
// We need to pass the TabContents to the dialog, because multiple tabs can hang
// and it needs to keep track of which tabs are currently hung.
void ShowHungRendererDialog(TabContents* contents);
void HideHungRendererDialog(TabContents* contents);

} // namespace browser

#endif  // CHROME_BROWSER_UI_BROWSER_DIALOGS_H_
