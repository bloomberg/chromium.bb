// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/network_message_observer.h"

#include "app/l10n_util.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

namespace chromeos {

NetworkMessageObserver::NetworkMessageObserver(Profile* profile)
  : notification_(profile, "network_connection.chromeos",
        IDR_NOTIFICATION_NETWORK_FAILED,
        l10n_util::GetStringUTF16(IDS_NETWORK_CONNECTION_ERROR_TITLE)) {
}

NetworkMessageObserver::~NetworkMessageObserver() {
  notification_.Hide();
}

void NetworkMessageObserver::NetworkChanged(NetworkLibrary* obj) {
  // TODO(chocobo): Display open networks notification here.
//    notification_.Show(l10n_util::GetStringFUTF16(
//        IDS_NETWORK_CONNECTION_ERROR_MESSAGE,
//        ASCIIToUTF16(network.name())), true);
}

}  // namespace chromeos
