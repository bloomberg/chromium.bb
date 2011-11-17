// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/sms_observer.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/notifications/system_notification.h"
#include "chrome/browser/profiles/profile.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/cros/chromeos_network.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

SmsObserver::SmsObserver(Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
  UpdateObservers(chromeos::CrosLibrary::Get()->GetNetworkLibrary());
}

SmsObserver::~SmsObserver() {
  NetworkLibrary* library = chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  library->RemoveNetworkManagerObserver(this);
  DisconnectAll();
}

void SmsObserver::UpdateObservers(NetworkLibrary* library) {
  // Guard against calls to libcros (http://crosbug.com/17863).
  if (!CrosLibrary::Get()->libcros_loaded())
    return;

  const CellularNetworkVector& networks = library->cellular_networks();
  // Remove monitors for networks that are not in the list anymore.
  for (ObserversMap::iterator it_observer = observers_.begin();
       it_observer != observers_.end();) {
    bool found = false;
    for (CellularNetworkVector::const_iterator it_network = networks.begin();
        it_network != networks.end(); ++it_network) {
      if (it_observer->first == (*it_network)->device_path()) {
        found = true;
        break;
      }
    }
    if (!found) {
      VLOG(1) << "Remove SMS monitor for " << it_observer->first;
      chromeos::DisconnectSMSMonitor(it_observer->second);
      observers_.erase(it_observer++);
    } else {
      ++it_observer;
    }
  }

  // Add monitors for new networks.
  for (CellularNetworkVector::const_iterator it_network = networks.begin();
       it_network != networks.end(); ++it_network) {
    const std::string& device_path((*it_network)->device_path());
    if (device_path.empty()) {
      LOG(WARNING) << "Cellular Network has empty device path: "
                   << (*it_network)->name();
      continue;
    }
    ObserversMap::iterator it_observer = observers_.find(device_path);
    if (it_observer == observers_.end()) {
      VLOG(1) << "Add SMS monitor for " << device_path;
      chromeos::SMSMonitor monitor =
          chromeos::MonitorSMS(device_path.c_str(), &StaticCallback, this);
      observers_.insert(ObserversMap::value_type(device_path, monitor));
    } else {
      VLOG(1) << "Already has SMS monitor for " << device_path;
    }
  }
}

void SmsObserver::DisconnectAll() {
  // Guard against calls to libcros (http://crosbug.com/17863).
  if (!CrosLibrary::Get()->libcros_loaded())
    return;

  for (ObserversMap::iterator it = observers_.begin();
       it != observers_.end(); ++it) {
    VLOG(1) << "Remove SMS monitor for " << it->first;
    chromeos::DisconnectSMSMonitor(it->second);
  }
  observers_.clear();
}

void SmsObserver::OnNetworkManagerChanged(NetworkLibrary* library) {
  UpdateObservers(library);
}

// static
void SmsObserver::StaticCallback(void* object,
                                 const char* modem_device_path,
                                 const SMS* message) {
  SmsObserver* monitor = static_cast<SmsObserver*>(object);
  monitor->OnNewMessage(modem_device_path, message);
}

void SmsObserver::OnNewMessage(const char* modem_device_path,
                               const SMS* message) {
  VLOG(1) << "New message notification from " << message->number
          << " text: " << message->text;

  SystemNotification note(
      profile_,
      "incoming _sms.chromeos",
      IDR_NOTIFICATION_SMS,
      l10n_util::GetStringFUTF16(
          IDS_SMS_NOTIFICATION_TITLE, UTF8ToUTF16(message->number)));

  note.Show(UTF8ToUTF16(message->text), true, false);
}

}  // namespace chromeos
