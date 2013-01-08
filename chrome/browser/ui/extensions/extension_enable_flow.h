// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_ENABLE_FLOW_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_ENABLE_FLOW_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_install_prompt.h"

class ExtensionEnableFlowDelegate;

// ExtensionEnableFlow performs an UI flow to enable a disabled/terminated
// extension. It calls its delegate when enabling is done or is aborted.
// Callback on the delegate might be called synchronously if there is no
// permission change while the extension is disabled/terminated (or the
// extension is enabled already). Otherwise, a re-enable install prompt is
// shown to user. The extension is enabled when user acknowledges it or the
// flow is aborted when user declines it.
class ExtensionEnableFlow : public ExtensionInstallPrompt::Delegate {
 public:
  ExtensionEnableFlow(Profile* profile,
                      const std::string& extension_id,
                      ExtensionEnableFlowDelegate* delegate);
  virtual ~ExtensionEnableFlow();

  // Starts the flow and the logic continues on |delegate_| after enabling is
  // finished or aborted.
  void Start();

  const std::string& extension_id() const { return extension_id_; }

 private:
  // ExtensionInstallPrompt::Delegate overrides:
  virtual void InstallUIProceed() OVERRIDE;
  virtual void InstallUIAbort(bool user_initiated) OVERRIDE;

  Profile* const profile_;
  const std::string extension_id_;
  ExtensionEnableFlowDelegate* const delegate_;  // Not owned.

  scoped_ptr<ExtensionInstallPrompt> prompt_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionEnableFlow);
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_ENABLE_FLOW_H_
