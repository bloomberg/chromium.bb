// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_HATS_HATS_NOTIFICATION_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_HATS_HATS_NOTIFICATION_CONTROLLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_delegate.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "ui/gfx/image/image_skia.h"

class Profile;
class NetworkState;

namespace gfx {
class Image;
}

namespace image_fetcher {
class ImageFetcher;
}

namespace chromeos {

// Happiness tracking survey (HaTS) notification controller is responsible for
// managing the HaTS notification that is displayed to the user.
class HatsNotificationController : public NotificationDelegate,
                                   public NetworkPortalDetector::Observer {
 public:
  static const char kDelegateId[];
  static const char kNotificationId[];
  static const char kImageFetcher1xId[];
  static const char kImageFetcher2xId[];
  static const char kGoogleIcon1xUrl[];
  static const char kGoogleIcon2xUrl[];

  explicit HatsNotificationController(
      Profile* profile,
      image_fetcher::ImageFetcher* image_fetcher = nullptr);

  // Returns true if the survey needs to be displayed for the given |profile|.
  static bool ShouldShowSurveyToProfile(Profile* profile);

 private:
  friend class HatsNotificationControllerTest;
  FRIEND_TEST_ALL_PREFIXES(HatsNotificationControllerTest,
                           NewDevice_ShouldNotShowNotification);
  FRIEND_TEST_ALL_PREFIXES(HatsNotificationControllerTest,
                           OldDevice_ShouldShowNotification);
  FRIEND_TEST_ALL_PREFIXES(HatsNotificationControllerTest,
                           NoInternet_DoNotShowNotification);
  FRIEND_TEST_ALL_PREFIXES(HatsNotificationControllerTest,
                           InternetConnected_ShowNotification);
  FRIEND_TEST_ALL_PREFIXES(HatsNotificationControllerTest,
                           DismissNotification_ShouldUpdatePref);

  ~HatsNotificationController() override;

  // NotificationDelegate overrides:
  void Initialize(bool is_new_device);
  void ButtonClick(int button_index) override;
  void Close(bool by_user) override;
  void Click() override;
  std::string id() const override;

  void OnImageFetched(const std::string& id, const gfx::Image& image);

  // NetworkPortalDetector::Observer override:
  void OnPortalDetectionCompleted(
      const NetworkState* network,
      const NetworkPortalDetector::CaptivePortalState& state) override;

  Notification* CreateNotification();
  void UpdateLastInteractionTime();

  Profile* profile_;
  // A count of requests that have been completed so far. This includes requests
  // that may have failed as well.
  int completed_requests_;
  std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher_;
  gfx::ImageSkia icon_;
  base::WeakPtrFactory<HatsNotificationController> weak_pointer_factory_;

  DISALLOW_COPY_AND_ASSIGN(HatsNotificationController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_HATS_HATS_NOTIFICATION_CONTROLLER_H_
