// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_ENABLE_FLOW_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_ENABLE_FLOW_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "content/public/browser/page_navigator.h"

class ExtensionEnableFlowDelegate;

namespace content {
class PageNavigator;
class WebContents;
}

// ExtensionEnableFlow performs an UI flow to enable a disabled/terminated
// extension. It calls its delegate when enabling is done or is aborted.
// Callback on the delegate might be called synchronously if there is no
// permission change while the extension is disabled/terminated (or the
// extension is enabled already). Otherwise, a re-enable install prompt is
// shown to user. The extension is enabled when user acknowledges it or the
// flow is aborted when user declines it.
class ExtensionEnableFlow : public ExtensionInstallPrompt::Delegate,
                            public content::PageNavigator {
 public:
  ExtensionEnableFlow(Profile* profile,
                      const std::string& extension_id,
                      ExtensionEnableFlowDelegate* delegate);
  virtual ~ExtensionEnableFlow();

  // Starts the flow and the logic continues on |delegate_| after enabling is
  // finished or aborted. Note that |delegate_| could be called synchronously
  // before this call returns when there is no need to show UI to finish the
  // enabling flow. Two variations of the flow are supported: one with a
  // parent WebContents and the other with a native parent window.
  void StartForWebContents(content::WebContents* parent_contents);
  void StartForNativeWindow(gfx::NativeWindow parent_window);

  const std::string& extension_id() const { return extension_id_; }

 private:
  // Runs the enable flow. Currently, it checks if there is permission
  // escalation while the extension is disabled/terminated. If no, enables the
  // extension and notify |delegate_| synchronously. Otherwise, creates an
  // ExtensionInstallPrompt and asks user to confirm.
  void Run();

  // Creates an ExtensionInstallPrompt in |prompt_|.
  void CreatePrompt();

  // ExtensionInstallPrompt::Delegate overrides:
  virtual void InstallUIProceed() OVERRIDE;
  virtual void InstallUIAbort(bool user_initiated) OVERRIDE;

  // content::PageNavigator overrides:
  virtual content::WebContents* OpenURL(
      const content::OpenURLParams& params) OVERRIDE;

  Profile* const profile_;
  const std::string extension_id_;
  ExtensionEnableFlowDelegate* const delegate_;  // Not owned.

  // Parent web contents for ExtensionInstallPrompt that may be created during
  // the flow. Note this is mutually exclusive with |parent_window_| below.
  content::WebContents* parent_contents_;

  // Parent native window for ExtensionInstallPrompt. Note this is mutually
  // exclusive with |parent_contents_| above.
  gfx::NativeWindow parent_window_;

  scoped_ptr<ExtensionInstallPrompt> prompt_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionEnableFlow);
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_ENABLE_FLOW_H_
