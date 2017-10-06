// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/session/app_terminating_stack_dumper.h"

#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "chrome/browser/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"

namespace chromeos {

AppTerminatingStackDumper::AppTerminatingStackDumper() {
  registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());
}

AppTerminatingStackDumper::~AppTerminatingStackDumper() = default;

void AppTerminatingStackDumper::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_APP_TERMINATING, type);

  // Tracks unexpected NOTIFICATION_APP_TERMINATING during user profile
  // loading for http://crbug.com/717585.
  base::debug::DumpWithoutCrashing();
}

}  // namespace chromeos
