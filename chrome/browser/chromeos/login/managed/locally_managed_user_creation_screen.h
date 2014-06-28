// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MANAGED_LOCALLY_MANAGED_USER_CREATION_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MANAGED_LOCALLY_MANAGED_USER_CREATION_SCREEN_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/camera_presence_notifier.h"
#include "chrome/browser/chromeos/login/managed/managed_user_creation_controller.h"
#include "chrome/browser/chromeos/login/screens/wizard_screen.h"
#include "chrome/browser/image_decoder.h"
#include "chrome/browser/supervised_user/supervised_user_sync_service.h"
#include "chrome/browser/ui/webui/chromeos/login/locally_managed_user_creation_screen_handler.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "ui/gfx/image/image_skia.h"

class Profile;

namespace chromeos {

class NetworkState;

// Class that controls screen showing ui for locally managed user creation.
class LocallyManagedUserCreationScreen
    : public WizardScreen,
      public LocallyManagedUserCreationScreenHandler::Delegate,
      public ManagedUserCreationController::StatusConsumer,
      public SupervisedUserSyncServiceObserver,
      public ImageDecoder::Delegate,
      public NetworkPortalDetector::Observer,
      public CameraPresenceNotifier::Observer {
 public:
  LocallyManagedUserCreationScreen(
      ScreenObserver* observer,
      LocallyManagedUserCreationScreenHandler* actor);
  virtual ~LocallyManagedUserCreationScreen();

  // Makes screen to show message about inconsistency in manager login flow
  // (e.g. password change detected, invalid OAuth token, etc).
  // Called when manager user is successfully authenticated, so ui elements
  // should result in forced logout.
  void ShowManagerInconsistentStateErrorScreen();

  // Called when authentication fails for manager with provided password.
  // Displays wrong password message on manager selection screen.
  void OnManagerLoginFailure();

  // Called when manager is successfully authenticated and account is in
  // consistent state.
  void OnManagerFullyAuthenticated(Profile* manager_profile);

  // Called when manager is successfully authenticated against cryptohome, but
  // OAUTH token validation hasn't completed yet.
  // Results in spinner indicating that creation is in process.
  void OnManagerCryptohomeAuthenticated();

  // Shows initial screen where managed user name/password are defined and
  // manager is selected.
  void ShowInitialScreen();

  // CameraPresenceNotifier::Observer implementation:
  virtual void OnCameraPresenceCheckDone(bool is_camera_present) OVERRIDE;

  // SupervisedUserSyncServiceObserver implementation
  virtual void OnSupervisedUserAcknowledged(
      const std::string& supervised_user_id) OVERRIDE {}
  virtual void OnSupervisedUsersSyncingStopped() OVERRIDE {}
  virtual void OnSupervisedUsersChanged() OVERRIDE;

  // WizardScreen implementation:
  virtual void PrepareToShow() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual std::string GetName() const OVERRIDE;

  // LocallyManagedUserCreationScreenHandler::Delegate implementation:
  virtual void OnActorDestroyed(LocallyManagedUserCreationScreenHandler* actor)
      OVERRIDE;
  virtual void CreateManagedUser(
      const base::string16& display_name,
      const std::string& managed_user_password) OVERRIDE;
  virtual void ImportManagedUser(const std::string& user_id) OVERRIDE;
  virtual void ImportManagedUserWithPassword(
      const std::string& user_id,
      const std::string& password) OVERRIDE;
  virtual void AuthenticateManager(
      const std::string& manager_id,
      const std::string& manager_password) OVERRIDE;
  virtual void AbortFlow() OVERRIDE;
  virtual void FinishFlow() OVERRIDE;
  virtual bool FindUserByDisplayName(const base::string16& display_name,
                                     std::string *out_id) const OVERRIDE;
  virtual void OnPageSelected(const std::string& page) OVERRIDE;

  // LocallyManagedUserController::StatusConsumer overrides.
  virtual void OnCreationError(ManagedUserCreationController::ErrorCode code)
      OVERRIDE;
  virtual void OnCreationTimeout() OVERRIDE;
  virtual void OnCreationSuccess() OVERRIDE;
  virtual void OnLongCreationWarning() OVERRIDE;

  // NetworkPortalDetector::Observer implementation:
  virtual void OnPortalDetectionCompleted(
          const NetworkState* network,
          const NetworkPortalDetector::CaptivePortalState& state) OVERRIDE;

  // TODO(antrim) : this is an explicit code duplications with UserImageScreen.
  // It should be removed by issue 251179.

  // LocallyManagedUserCreationScreenHandler::Delegate (image) implementation:
  virtual void OnPhotoTaken(const std::string& raw_data) OVERRIDE;
  virtual void OnImageSelected(const std::string& image_url,
                               const std::string& image_type) OVERRIDE;
  virtual void OnImageAccepted() OVERRIDE;
  // ImageDecoder::Delegate overrides:
  virtual void OnImageDecoded(const ImageDecoder* decoder,
                              const SkBitmap& decoded_image) OVERRIDE;
  virtual void OnDecodeImageFailed(const ImageDecoder* decoder) OVERRIDE;

 private:
  void ApplyPicture();
  void OnGetManagedUsers(const base::DictionaryValue* users);

  base::WeakPtrFactory<LocallyManagedUserCreationScreen> weak_factory_;
  LocallyManagedUserCreationScreenHandler* actor_;

  scoped_ptr<ManagedUserCreationController> controller_;
  scoped_ptr<base::DictionaryValue> existing_users_;

  bool on_error_screen_;
  std::string last_page_;

  SupervisedUserSyncService* sync_service_;

  gfx::ImageSkia user_photo_;
  scoped_refptr<ImageDecoder> image_decoder_;
  bool apply_photo_after_decoding_;
  int selected_image_;

  DISALLOW_COPY_AND_ASSIGN(LocallyManagedUserCreationScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MANAGED_LOCALLY_MANAGED_USER_CREATION_SCREEN_H_

