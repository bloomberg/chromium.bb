// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_HTML_DIALOG_WINDOW_CONTROLLER_CPPSAFE_H_
#define CHROME_BROWSER_COCOA_HTML_DIALOG_WINDOW_CONTROLLER_CPPSAFE_H_

// We declare this in a separate file that is safe for including in C++ code.

// TODO(akalin): It would be nice if there were a platform-agnostic way to
// create a browser-independent HTML dialog.  However, this would require
// some invasive changes on the Windows/Linux side.  Remove this file once
// We have this platform-agnostic API.

namespace html_dialog_window_controller {

// Creates and shows an HtmlDialogWindowController with the given
// delegate and profile. The window is automatically destroyed when it is
// closed.
void ShowHtmlDialog(HtmlDialogUIDelegate* delegate, Profile* profile);

}  // namespace html_dialog_window_controller

#endif  // CHROME_BROWSER_COCOA_HTML_DIALOG_WINDOW_CONTROLLER_CPPSAFE_H_

