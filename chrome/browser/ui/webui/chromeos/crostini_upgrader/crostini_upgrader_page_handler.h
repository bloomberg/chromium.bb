// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_CROSTINI_UPGRADER_CROSTINI_UPGRADER_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_CROSTINI_UPGRADER_CROSTINI_UPGRADER_PAGE_HANDLER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/crostini/crostini_upgrader_ui_delegate.h"
#include "chrome/browser/ui/webui/chromeos/crostini_upgrader/crostini_upgrader.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class WebContents;
}  // namespace content

namespace chromeos {

class CrostiniUpgraderPageHandler
    : public chromeos::crostini_upgrader::mojom::PageHandler,
      public crostini::CrostiniUpgraderUIObserver {
 public:
  CrostiniUpgraderPageHandler(
      content::WebContents* web_contents,
      crostini::CrostiniUpgraderUIDelegate* upgrader_ui_delegate,
      mojo::PendingReceiver<chromeos::crostini_upgrader::mojom::PageHandler>
          pending_page_handler,
      mojo::PendingRemote<chromeos::crostini_upgrader::mojom::Page>
          pending_page,
      base::OnceClosure close_dialog_callback,
      base::OnceCallback<void(bool)> launch_callback);
  ~CrostiniUpgraderPageHandler() override;

  // chromeos::crostini_upgrader::mojom::PageHandler:
  void Backup(bool show_file_chooser) override;
  void StartPrechecks() override;
  void Upgrade() override;
  void Restore() override;
  void Cancel() override;
  void CancelBeforeStart() override;
  void Close() override;
  void Launch() override;

  // CrostiniUpgraderUIObserver
  void OnBackupMaybeStarted(bool did_start) override;
  void OnBackupProgress(int percent) override;
  void OnBackupSucceeded(bool was_cancelled) override;
  void OnBackupFailed() override;
  void PrecheckStatus(chromeos::crostini_upgrader::mojom::UpgradePrecheckStatus
                          status) override;
  void OnUpgradeProgress(const std::vector<std::string>& messages) override;
  void OnUpgradeSucceeded() override;
  void OnUpgradeFailed() override;
  void OnRestoreProgress(int percent) override;
  void OnRestoreSucceeded() override;
  void OnRestoreFailed() override;
  void OnCanceled() override;

 private:
  // The chrome://crostini-upgrader WebUI that triggered the upgrade.
  // Used to parent open/save dialogs.
  content::WebContents* web_contents_;                          // Not owned.
  crostini::CrostiniUpgraderUIDelegate* upgrader_ui_delegate_;  // Not owned.
  mojo::Receiver<chromeos::crostini_upgrader::mojom::PageHandler> receiver_;
  mojo::Remote<chromeos::crostini_upgrader::mojom::Page> page_;
  base::OnceClosure close_dialog_callback_;
  base::OnceCallback<void(bool)> launch_callback_;
  // Will we need to restart the container as part of launch_callback?
  // |restart_required_| is true unless the user cancels before starting the
  // upgrade.
  bool restart_required_ = true;

  base::WeakPtrFactory<CrostiniUpgraderPageHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CrostiniUpgraderPageHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_CROSTINI_UPGRADER_CROSTINI_UPGRADER_PAGE_HANDLER_H_
