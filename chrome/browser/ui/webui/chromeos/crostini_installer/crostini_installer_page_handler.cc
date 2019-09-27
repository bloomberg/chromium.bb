// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/crostini_installer/crostini_installer_page_handler.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/chromeos/crostini/crostini_installer_ui_delegate.h"

namespace chromeos {

CrostiniInstallerPageHandler::CrostiniInstallerPageHandler(
    crostini::CrostiniInstallerUIDelegate* installer_ui_delegate,
    mojo::PendingReceiver<chromeos::crostini_installer::mojom::PageHandler>
        pending_page_handler,
    mojo::PendingRemote<chromeos::crostini_installer::mojom::Page> pending_page,
    base::OnceClosure close_dialog_callback)
    : installer_ui_delegate_{installer_ui_delegate},
      receiver_{this, std::move(pending_page_handler)},
      page_{std::move(pending_page)},
      close_dialog_callback_{std::move(close_dialog_callback)} {}

CrostiniInstallerPageHandler::~CrostiniInstallerPageHandler() = default;

void CrostiniInstallerPageHandler::Install() {
  installer_ui_delegate_->Install(
      base::BindRepeating(&CrostiniInstallerPageHandler::OnProgressUpdate,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&CrostiniInstallerPageHandler::OnInstallFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniInstallerPageHandler::Cancel() {
  installer_ui_delegate_->Cancel(
      base::BindOnce(&CrostiniInstallerPageHandler::OnCanceled,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniInstallerPageHandler::CancelBeforeStart() {
  installer_ui_delegate_->CancelBeforeStart();
}

void CrostiniInstallerPageHandler::Close() {
  std::move(close_dialog_callback_).Run();
}

void CrostiniInstallerPageHandler::OnProgressUpdate(
    crostini::mojom::InstallerState installer_state,
    double progress_fraction) {
  // TODO(lxj)
}

void CrostiniInstallerPageHandler::OnInstallFinished(
    crostini::mojom::InstallerError error) {
  // TODO(lxj)
}

void CrostiniInstallerPageHandler::OnCanceled() {
  // TODO(lxj)
}

}  // namespace chromeos
