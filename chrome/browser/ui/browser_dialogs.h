// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_DIALOGS_H_
#define CHROME_BROWSER_UI_BROWSER_DIALOGS_H_

#include "base/callback.h"
#include "content/public/common/signed_certificate_timestamp_id_and_status.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/native_widget_types.h"

class Browser;
class Profile;
class SkBitmap;

namespace content {
class BrowserContext;
class ColorChooser;
class WebContents;
}

namespace extensions {
class Extension;
}

namespace ui {
class WebDialogDelegate;
}

namespace chrome {

// Creates and shows an HTML dialog with the given delegate and context.
// The window is automatically destroyed when it is closed.
// Returns the created window.
//
// Make sure to use the returned window only when you know it is safe
// to do so, i.e. before OnDialogClosed() is called on the delegate.
gfx::NativeWindow ShowWebDialog(gfx::NativeView parent,
                                content::BrowserContext* context,
                                ui::WebDialogDelegate* delegate);

// Creates the ExtensionInstalledBubble and schedules it to be shown once
// the extension has loaded. |extension| is the installed extension. |browser|
// is the browser window which will host the bubble. |icon| is the install
// icon of the extension.
void ShowExtensionInstalledBubble(const extensions::Extension* extension,
                                  Browser* browser,
                                  const SkBitmap& icon);

// Shows or hides the Task Manager. |browser| can be NULL when called from Ash.
void ShowTaskManager(Browser* browser);
void HideTaskManager();

#if !defined(OS_MACOSX)
// Shows the create web app shortcut dialog box.
void ShowCreateWebAppShortcutsDialog(gfx::NativeWindow parent_window,
                                     content::WebContents* web_contents);
#endif

// Shows the create chrome app shortcut dialog box.
// |close_callback| may be null.
void ShowCreateChromeAppShortcutsDialog(
    gfx::NativeWindow parent_window,
    Profile* profile,
    const extensions::Extension* app,
    const base::Callback<void(bool /* created */)>& close_callback);

// Shows a color chooser that reports to the given WebContents.
content::ColorChooser* ShowColorChooser(content::WebContents* web_contents,
                                        SkColor initial_color);

// Shows the Signed Certificate Timestamps viewer, to view the signed
// certificate timestamps in |sct_ids_list|
void ShowSignedCertificateTimestampsViewer(
    content::WebContents* web_contents,
    const content::SignedCertificateTimestampIDStatusList& sct_ids_list);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_DIALOGS_H_
