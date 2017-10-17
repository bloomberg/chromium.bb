// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PRINT_JOB_NOTIFICATION_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PRINT_JOB_NOTIFICATION_H_

#include <memory>
#include <string>
#include <vector>

#include "ui/gfx/image/image.h"

class Profile;

namespace message_center {
class Notification;
}

namespace chromeos {

class CupsPrintJob;
class CupsPrintJobNotificationManager;

// CupsPrintJobNotification is used to update the notification of a print job
// according to its state and respond to the user's action.
class CupsPrintJobNotification {
 public:
  enum class ButtonCommand {
    CANCEL_PRINTING,
    PAUSE_PRINTING,
    RESUME_PRINTING,
    GET_HELP,
  };

  CupsPrintJobNotification(CupsPrintJobNotificationManager* manager,
                           CupsPrintJob* print_job,
                           Profile* profile);
  ~CupsPrintJobNotification();

  void OnPrintJobStatusUpdated();

  void CloseNotificationByUser();
  void ClickOnNotificationButton(int button_index);

 private:
  // Update the notification based on the print job's status.
  void UpdateNotification();
  void UpdateNotificationTitle();
  void UpdateNotificationIcon();
  void UpdateNotificationBodyMessage();
  void UpdateNotificationType();
  void UpdateNotificationButtons();

  // Returns the buttons according to the print job's current status.
  std::unique_ptr<std::vector<ButtonCommand>> GetButtonCommands() const;
  base::string16 GetButtonLabel(ButtonCommand button) const;
  gfx::Image GetButtonIcon(ButtonCommand button) const;

  CupsPrintJobNotificationManager* notification_manager_;
  std::unique_ptr<message_center::Notification> notification_;
  std::string notification_id_;
  CupsPrintJob* print_job_;
  Profile* profile_;

  // If the notification has been closed in the middle of printing or not. If it
  // is true, then prevent the following print job progress update after close,
  // and only show the print job done or failed notification.
  bool closed_in_middle_ = false;

  // If this is true, the user cancelled the job using the cancel button and
  // should not be notified of events.
  bool cancelled_by_user_ = false;

  // Maintains a list of button actions according to the print job's current
  // status.
  std::unique_ptr<std::vector<ButtonCommand>> button_commands_;

  DISALLOW_COPY_AND_ASSIGN(CupsPrintJobNotification);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_CUPS_PRINT_JOB_NOTIFICATION_H_
