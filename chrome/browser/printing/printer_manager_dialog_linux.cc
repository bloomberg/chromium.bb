// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/printer_manager_dialog.h"

#include "base/bind.h"
#include "base/environment.h"
#include "base/nix/xdg_util.h"
#include "base/process_util.h"
#include "content/public/browser/browser_thread.h"

using base::Environment;
using content::BrowserThread;

namespace {

// KDE printer config command ("system-config-printer-kde") causes the
// OptionWidget to crash (https://bugs.kde.org/show_bug.cgi?id=271957).
// Therefore, use GNOME printer config command for KDE.
const char kGNOMEPrinterConfigCommand[] = "system-config-printer";

// Detect the command based on the deskop environment and open the printer
// manager dialog.
void DetectAndOpenPrinterConfigDialog() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  scoped_ptr<Environment> env(Environment::Create());

  const char* command = NULL;
  switch (base::nix::GetDesktopEnvironment(env.get())) {
    case base::nix::DESKTOP_ENVIRONMENT_GNOME:
    case base::nix::DESKTOP_ENVIRONMENT_KDE3:
    case base::nix::DESKTOP_ENVIRONMENT_KDE4:
      command = kGNOMEPrinterConfigCommand;
      break;
    case base::nix::DESKTOP_ENVIRONMENT_XFCE:
    case base::nix::DESKTOP_ENVIRONMENT_OTHER:
      break;
    default:
      NOTREACHED();
      break;
  }

  if (!command) {
    LOG(ERROR) << "Failed to detect the command to open printer config dialog";
    return;
  }

  std::vector<std::string> argv;
  argv.push_back(command);
  base::ProcessHandle handle;
  if (!base::LaunchProcess(argv, base::LaunchOptions(), &handle)) {
    LOG(ERROR) << "Failed to open printer manager dialog ";
    return;
  }
  base::EnsureProcessGetsReaped(handle);
}

}  // anonymous namespace

namespace printing {

void PrinterManagerDialog::ShowPrinterManagerDialog() {
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(&DetectAndOpenPrinterConfigDialog));
}

}  // namespace printing
