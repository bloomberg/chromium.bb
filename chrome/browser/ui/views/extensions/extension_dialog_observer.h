// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_DIALOG_OBSERVER_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_DIALOG_OBSERVER_H_
#pragma once

class ExtensionDialog;

// Observer to ExtensionDialog events.
class ExtensionDialogObserver {
 public:
  ExtensionDialogObserver();
  virtual ~ExtensionDialogObserver();

  // Called when the ExtensionDialog is closing. Note that it
  // is ref-counted, and thus will be released shortly after
  // making this delegate call.
  virtual void ExtensionDialogClosing(ExtensionDialog* popup) = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_DIALOG_OBSERVER_H_
