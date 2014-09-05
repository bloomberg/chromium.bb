// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_USER_IMAGE_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_USER_IMAGE_SCREEN_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/camera_presence_notifier.h"
#include "chrome/browser/chromeos/login/screens/user_image_screen_actor.h"
#include "chrome/browser/chromeos/login/screens/wizard_screen.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_sync_observer.h"
#include "chrome/browser/image_decoder.h"
#include "components/user_manager/user.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace base {
class Timer;
class Value;
};

namespace policy {
class PolicyChangeRegistrar;
}

namespace chromeos {

class UserImageManager;
class ScreenManager;

class UserImageScreen: public WizardScreen,
                       public UserImageScreenActor::Delegate,
                       public ImageDecoder::Delegate,
                       public content::NotificationObserver,
                       public UserImageSyncObserver::Observer,
                       public CameraPresenceNotifier::Observer {
 public:
  UserImageScreen(ScreenObserver* screen_observer,
                  UserImageScreenActor* actor);
  virtual ~UserImageScreen();

  static UserImageScreen* Get(ScreenManager* manager);

  // WizardScreen implementation:
  virtual void PrepareToShow() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual std::string GetName() const OVERRIDE;

  // UserImageScreenActor::Delegate implementation:
  virtual void OnScreenReady() OVERRIDE;
  virtual void OnPhotoTaken(const std::string& raw_data) OVERRIDE;
  virtual void OnImageSelected(const std::string& image_url,
                               const std::string& image_type,
                               bool is_user_selection) OVERRIDE;
  virtual void OnImageAccepted() OVERRIDE;
  virtual void OnActorDestroyed(UserImageScreenActor* actor) OVERRIDE;

  virtual bool profile_picture_absent() OVERRIDE;
  virtual int selected_image() OVERRIDE;
  virtual std::string profile_picture_data_url() OVERRIDE;

  // content::NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // ImageDecoder::Delegate implementation:
  virtual void OnImageDecoded(const ImageDecoder* decoder,
                              const SkBitmap& decoded_image) OVERRIDE;
  virtual void OnDecodeImageFailed(const ImageDecoder* decoder) OVERRIDE;

  // CameraPresenceNotifier::Observer implementation:
  virtual void OnCameraPresenceCheckDone(bool is_camera_present) OVERRIDE;

  // UserImageSyncObserver::Observer implementation:
  virtual void OnInitialSync(bool local_image_updated) OVERRIDE;

  bool user_selected_image() const { return user_has_selected_image_; }

 private:
  // Called when whaiting for sync timed out.
  void OnSyncTimeout();

  bool IsWaitingForSync() const;

  // Called when the policy::key::kUserAvatarImage policy changes while the
  // screen is being shown. If the policy is set, closes the screen because the
  // user is not allowed to override a policy-set image.
  void OnUserImagePolicyChanged(const base::Value* previous,
                                const base::Value* current);

  // Returns current user.
  const user_manager::User* GetUser();

  // Returns UserImageManager for the current user.
  UserImageManager* GetUserImageManager();

  // Returns UserImageSyncObserver for the current user.
  UserImageSyncObserver* GetSyncObserver();

  // Called when it's decided not to skip the screen.
  void HideCurtain();

  // Closes the screen.
  void ExitScreen();

  content::NotificationRegistrar notification_registrar_;

  scoped_ptr<policy::PolicyChangeRegistrar> policy_registrar_;

  UserImageScreenActor* actor_;

  // Last ImageDecoder instance used to decode an image blob received by
  // HandlePhotoTaken.
  scoped_refptr<ImageDecoder> image_decoder_;

  // Last user photo, if taken.
  gfx::ImageSkia user_photo_;

  // If |true|, decoded photo should be immediately accepeted (i.e., both
  // HandleTakePhoto and HandleImageAccepted have already been called but we're
  // still waiting for  photo image decoding to finish.
  bool accept_photo_after_decoding_;

  // Index of the selected user image.
  int selected_image_;

  // Encoded profile picture.
  std::string profile_picture_data_url_;

  // True if user has no custom profile picture.
  bool profile_picture_absent_;

  // Timer used for waiting for user image sync.
  scoped_ptr<base::Timer> sync_timer_;

  // If screen ready to be shown.
  bool is_screen_ready_;

  // True if user has explicitly selected some image.
  bool user_has_selected_image_;

  DISALLOW_COPY_AND_ASSIGN(UserImageScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_USER_IMAGE_SCREEN_H_
