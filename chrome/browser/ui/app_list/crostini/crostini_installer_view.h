// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_CROSTINI_CROSTINI_INSTALLER_VIEW_H_
#define CHROME_BROWSER_UI_APP_LIST_CROSTINI_CROSTINI_INSTALLER_VIEW_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/component_updater/cros_component_installer.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Label;
class ProgressBar;
}  // namespace views

namespace crostini {
enum class ConciergeClientResult;
}  // namespace crostini

class CrostiniAppItem;
class Profile;

// The Crostini installer. Provides details about Crostini to the user and
// installs it if the user chooses to do so.
class CrostiniInstallerView : public views::DialogDelegateView {
 public:
  static void Show(const CrostiniAppItem* app_item, Profile* profile);

  // views::DialogDelegateView:
  int GetDialogButtons() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  base::string16 GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  bool Accept() override;
  gfx::Size CalculatePreferredSize() const override;

  static CrostiniInstallerView* GetActiveViewForTesting();

 private:
  CrostiniInstallerView(const CrostiniAppItem* app_item, Profile* profile);
  ~CrostiniInstallerView() override;

  void HandleError(const base::string16& error_message);

  void InstallImageLoader();
  static void InstallImageLoaderFinished(
      base::WeakPtr<CrostiniInstallerView> weak_ptr,
      component_updater::CrOSComponentManager::Error error,
      const base::FilePath& result);
  void InstallImageLoaderFinishedOnUIThread(
      component_updater::CrOSComponentManager::Error error,
      const base::FilePath& result);

  void StartVmConcierge();
  void StartVmConciergeFinished(bool success);

  void CreateDiskImage();
  void CreateDiskImageFinished(crostini::ConciergeClientResult result,
                               const base::FilePath& result_path);

  void StartTerminaVm(const base::FilePath& result_path);
  void StartTerminaVmFinished(crostini::ConciergeClientResult result);

  void StartContainer();
  void StartContainerFinished(crostini::ConciergeClientResult result);

  void ShowLoginShell();

  void StepProgressBar();

  enum class State {
    PROMPT,  // Prompting the user to allow installation.
    ERROR,   // Something unexpected happened.
    // We automatically progress through the following steps.
    INSTALL_START,         // The user has just clicked 'Install'.
    INSTALL_IMAGE_LOADER,  // Loading the Termina VM component.
    START_CONCIERGE,       // Starting the Concierge D-Bus client.
    CREATE_DISK_IMAGE,     // Creating the image for the Termina VM.
    START_TERMINA_VM,      // Starting the Termina VM.
    START_CONTAINER,       // Starting the container inside the Termina VM.
    SHOW_LOGIN_SHELL,      // Showing a new crosh window.
    INSTALL_END = SHOW_LOGIN_SHELL,  // Marker enum for last install state.
  };
  State state_ = State::PROMPT;
  views::Label* message_label_ = nullptr;
  views::ProgressBar* progress_bar_ = nullptr;
  base::string16 app_name_;
  Profile* profile_;
  std::string cryptohome_id_;
  std::string container_user_name_;

  base::WeakPtrFactory<CrostiniInstallerView> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniInstallerView);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_CROSTINI_CROSTINI_INSTALLER_VIEW_H_
