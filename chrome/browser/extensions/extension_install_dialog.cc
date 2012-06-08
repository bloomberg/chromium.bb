// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_dialog.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "ui/gfx/image/image.h"

namespace {

// A flag used for SetExtensionInstallDialogAutoConfirmForTests
enum AutoConfirmForTest {
  DO_NOT_SKIP = 0,
  PROCEED,
  ABORT
};

void AutoConfirmTask(ExtensionInstallPrompt::Delegate* delegate, bool proceed) {
  if (proceed)
    delegate->InstallUIProceed();
  else
    delegate->InstallUIAbort(true);
}

void DoAutoConfirm(AutoConfirmForTest setting,
                   ExtensionInstallPrompt::Delegate* delegate) {
  bool proceed = (setting == PROCEED);
  // We use PostTask instead of calling the delegate directly here, because in
  // the real implementations it's highly likely the message loop will be
  // pumping a few times before the user clicks accept or cancel.
  MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&AutoConfirmTask, delegate, proceed));
}

AutoConfirmForTest CheckAutoConfirmCommandLineSwitch() {
  const CommandLine* cmdline = CommandLine::ForCurrentProcess();
  if (!cmdline->HasSwitch(switches::kAppsGalleryInstallAutoConfirmForTests))
    return DO_NOT_SKIP;
  std::string value = cmdline->GetSwitchValueASCII(
      switches::kAppsGalleryInstallAutoConfirmForTests);
  if (value == "accept")
    return PROCEED;
  else if (value == "cancel")
    return ABORT;
  else
    NOTREACHED();
  return DO_NOT_SKIP;
}

}  // namespace

void ShowExtensionInstallDialog(Profile* profile,
                                ExtensionInstallPrompt::Delegate* delegate,
                                const ExtensionInstallPrompt::Prompt& prompt) {
  AutoConfirmForTest auto_confirm = CheckAutoConfirmCommandLineSwitch();
  if (auto_confirm != DO_NOT_SKIP) {
    DoAutoConfirm(auto_confirm, delegate);
    return;
  }
  ShowExtensionInstallDialogImpl(profile, delegate, prompt);
}
