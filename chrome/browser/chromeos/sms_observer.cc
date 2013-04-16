// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/sms_observer.h"

#include "ash/shell.h"
#include "ash/system/chromeos/network/sms_observer.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chromeos/network/cros_network_functions.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
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
      delete it_observer->second;
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
      CrosNetworkWatcher* watcher =
          CrosMonitorSMS(device_path,
                         base::Bind(&SmsObserver::OnNewMessage,
                                    base::Unretained(this)));
      observers_.insert(ObserversMap::value_type(device_path, watcher));
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
    delete it->second;
  }
  observers_.clear();
}

void SmsObserver::OnNetworkManagerChanged(NetworkLibrary* library) {
  UpdateObservers(library);
}

void SmsObserver::OnNewMessage(const std::string& modem_device_path,
                               const SMS& message) {
  VLOG(1) << "New message notification from " << message.number
          << " text: " << message.text;

  // Don't show empty messages. Such messages most probably "Message Waiting
  // Indication" and it should be determined by data-coding-scheme,
  // see crosbug.com/27883. But this field is not exposed from libcros.
  // TODO(dpokuhin): when libcros refactoring done, implement check for
  // "Message Waiting Indication" to filter out such messages.
  if (message.text.empty())
    return;

  // Add an Ash SMS notification. TODO(stevenjb): Replace this code with
  // NetworkSmsHandler when fixed: crbug.com/133416.
  base::DictionaryValue dict;
  dict.SetString(ash::kSmsNumberKey, message.number);
  dict.SetString(ash::kSmsTextKey, message.text);
  ash::Shell::GetInstance()->system_tray_notifier()->NotifyAddSmsMessage(dict);
}

}  // namespace chromeos
