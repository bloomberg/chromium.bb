// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_ERROR_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_ERROR_CONTROLLER_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_error_ui.h"
#include "extensions/common/extension_set.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// The controller for the ExtensionErrorUI. This examines extensions for any
// blacklisted or external extensions in order to notify the user with an error.
// On acceptance, this will acknowledge the extensions.
class ExtensionErrorController : public ExtensionErrorUI::Delegate {
 public:
  typedef ExtensionErrorUI* (*UICreateMethod)(ExtensionErrorUI::Delegate*);

  ExtensionErrorController(content::BrowserContext* context, bool is_first_run);
  virtual ~ExtensionErrorController();

  void ShowErrorIfNeeded();

  // Set the factory method for creating a new ExtensionErrorUI.
  static void SetUICreateMethodForTesting(UICreateMethod method);

 private:
  // ExtensionErrorUI::Delegate implementation:
  virtual content::BrowserContext* GetContext() OVERRIDE;
  virtual const ExtensionSet& GetExternalExtensions() OVERRIDE;
  virtual const ExtensionSet& GetBlacklistedExtensions() OVERRIDE;
  virtual void OnAlertDetails() OVERRIDE;
  virtual void OnAlertAccept() OVERRIDE;
  virtual void OnAlertClosed() OVERRIDE;

  // Find any extensions that the user should be alerted about (like blacklisted
  // extensions).
  void IdentifyAlertableExtensions();

  // TODO(rdevlin.cronin): We never seem to use |external_extensions_| here,
  // but we do warn about them. Investigate more.
  ExtensionSet external_extensions_;

  // The extensions that are blacklisted and need user approval.
  ExtensionSet blacklisted_extensions_;

  // The UI component of this controller.
  scoped_ptr<ExtensionErrorUI> error_ui_;

  // The BrowserContext with which we are associated.
  content::BrowserContext* browser_context_;

  // Whether or not this is the first run. If it is, we avoid noisy errors, and
  // silently acknowledge blacklisted extensions.
  bool is_first_run_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionErrorController);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_ERROR_CONTROLLER_H_
