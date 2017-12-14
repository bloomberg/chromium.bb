// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/printer_manager_dialog.h"

#include <memory>

#include "base/bind.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/nix/xdg_util.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_restrictions.h"

namespace {

// KDE printer config command ("system-config-printer-kde") causes the
// OptionWidget to crash (https://bugs.kde.org/show_bug.cgi?id=271957).
// Therefore, use GNOME printer config command for KDE.
const char* const kSystemConfigPrinterCommand[] = {"system-config-printer",
                                                   nullptr};

const char* const kGnomeControlCenterPrintersCommand[] = {
    "gnome-control-center", "printers", nullptr};

// Returns true if the dialog was opened successfully.
bool OpenPrinterConfigDialog(const char* const* command) {
  DCHECK(command);
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  if (!base::ExecutableExistsInPath(env.get(), *command))
    return false;
  std::vector<std::string> argv;
  while (*command)
    argv.push_back(*command++);
  base::Process process = base::LaunchProcess(argv, base::LaunchOptions());
  if (!process.IsValid())
    return false;
  base::EnsureProcessGetsReaped(process.Pid());
  return true;
}

// Detect the command based on the deskop environment and open the printer
// manager dialog.
void DetectAndOpenPrinterConfigDialog() {
  base::AssertBlockingAllowed();
  std::unique_ptr<base::Environment> env(base::Environment::Create());

  bool opened = false;
  switch (base::nix::GetDesktopEnvironment(env.get())) {
    case base::nix::DESKTOP_ENVIRONMENT_GNOME:
      opened = OpenPrinterConfigDialog(kSystemConfigPrinterCommand) ||
               OpenPrinterConfigDialog(kGnomeControlCenterPrintersCommand);
      break;
    case base::nix::DESKTOP_ENVIRONMENT_CINNAMON:
    case base::nix::DESKTOP_ENVIRONMENT_KDE3:
    case base::nix::DESKTOP_ENVIRONMENT_KDE4:
    case base::nix::DESKTOP_ENVIRONMENT_KDE5:
    case base::nix::DESKTOP_ENVIRONMENT_PANTHEON:
    case base::nix::DESKTOP_ENVIRONMENT_UNITY:
    case base::nix::DESKTOP_ENVIRONMENT_XFCE:
      opened = OpenPrinterConfigDialog(kSystemConfigPrinterCommand);
      break;
    case base::nix::DESKTOP_ENVIRONMENT_OTHER:
      LOG(ERROR)
          << "Failed to detect the command to open printer config dialog";
      return;
  }
  LOG_IF(ERROR, !opened) << "Failed to open printer manager dialog ";
}

}  // namespace

namespace printing {

void PrinterManagerDialog::ShowPrinterManagerDialog() {
  base::PostTaskWithTraits(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&DetectAndOpenPrinterConfigDialog));
}

}  // namespace printing
