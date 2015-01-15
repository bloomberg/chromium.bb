// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_UPGRADE_DETECTOR_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_UPGRADE_DETECTOR_CHROMEOS_H_

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/upgrade_detector.h"
#include "chromeos/dbus/update_engine_client.h"

template <typename T> struct DefaultSingletonTraits;

class UpgradeDetectorChromeos : public UpgradeDetector,
                                public chromeos::UpdateEngineClient::Observer {
 public:
  ~UpgradeDetectorChromeos() override;

  static UpgradeDetectorChromeos* GetInstance();

  // Initializes the object. Starts observing changes from the update
  // engine.
  void Init();

  // Shuts down the object. Stops observing observe changes from the
  // update engine.
  void Shutdown();

 private:
  friend struct DefaultSingletonTraits<UpgradeDetectorChromeos>;
  class ChannelsRequester;

  UpgradeDetectorChromeos();

  // chromeos::UpdateEngineClient::Observer implementation.
  void UpdateStatusChanged(
      const chromeos::UpdateEngineClient::Status& status) override;

  // The function that sends out a notification (after a certain time has
  // elapsed) that lets the rest of the UI know we should start notifying the
  // user that a new version is available.
  void NotifyOnUpgrade();

  void OnChannelsReceived(const std::string& current_channel,
                          const std::string& target_channel);

  // After we detect an upgrade we start a recurring timer to see if enough time
  // has passed and we should start notifying the user.
  base::RepeatingTimer<UpgradeDetectorChromeos> upgrade_notification_timer_;
  bool initialized_;
  base::Time upgrade_detected_time_;

  scoped_ptr<ChannelsRequester> channels_requester_;

  base::WeakPtrFactory<UpgradeDetectorChromeos> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UpgradeDetectorChromeos);
};

#endif  // CHROME_BROWSER_CHROMEOS_UPGRADE_DETECTOR_CHROMEOS_H_
