// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_update.h"

#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "content/browser/browser_thread.h"
#include "third_party/cros/chromeos_update.h"
#include "views/window/window.h"

using views::Window;

////////////////////////////////////////////////////////////////////////////////
// GoogleUpdate, public:

GoogleUpdate::GoogleUpdate()
    : listener_(NULL) {
  chromeos::CrosLibrary::Get()->EnsureLoaded();
}

GoogleUpdate::~GoogleUpdate() {
}

////////////////////////////////////////////////////////////////////////////////
// GoogleUpdate, views::DialogDelegate implementation:

void GoogleUpdate::CheckForUpdate(bool install_if_newer, Window* window) {
  // We need to shunt this request over to InitiateGoogleUpdateCheck and have
  // it run in the file thread.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this, &GoogleUpdate::InitiateGoogleUpdateCheck, install_if_newer,
          window, MessageLoop::current()));
}

////////////////////////////////////////////////////////////////////////////////
// GoogleUpdate, private:

bool GoogleUpdate::InitiateGoogleUpdateCheck(bool install_if_newer,
                                             Window* window,
                                             MessageLoop* main_loop) {
  chromeos::UpdateInformation result;
  bool success = false;

  if (install_if_newer) {
    // Possible Results:
    //  UPGRADE_SUCCESSFUL
    //  UPGRADE_ALREADY_UP_TO_DATE
    //  UPGRADE_ERROR
    if (chromeos::Update) {
      success = chromeos::Update(&result);
    }
  } else {
    // Possible Results:
    //  UPGRADE_ALREADY_UP_TO_DATE
    //  UPGRADE_IS_AVAILABLE
    //  UPGRADE_ERROR
    if (chromeos::CheckForUpdate) {
      success = chromeos::CheckForUpdate(&result);
    }
    if (result.version_) {
      UTF8ToWide(result.version_, std::strlen(result.version_),
                 &version_available_);
    }
  }

  // Map chromeos::UpdateStatus to GoogleUpdateUpgradeResult

  GoogleUpdateUpgradeResult final = UPGRADE_ERROR;

  switch (result.status_) {
    case chromeos::UPDATE_ERROR:
      final = UPGRADE_ERROR;
      break;
    case chromeos::UPDATE_IS_AVAILABLE:
      final = UPGRADE_IS_AVAILABLE;
      break;
    case chromeos::UPDATE_SUCCESSFUL:
      final = UPGRADE_SUCCESSFUL;
      break;
    case chromeos::UPDATE_ALREADY_UP_TO_DATE:
      final = UPGRADE_ALREADY_UP_TO_DATE;
      break;
    default:
      // UPGRADE_ERROR
      break;
  }

  // Post the results as a task since this is run on a thread.

  main_loop->PostTask(FROM_HERE, NewRunnableMethod(this,
      &GoogleUpdate::ReportResults, final, success
      ?  GOOGLE_UPDATE_NO_ERROR : GOOGLE_UPDATE_ERROR_UPDATING));

  return true;
}

void GoogleUpdate::ReportResults(GoogleUpdateUpgradeResult results,
                                 GoogleUpdateErrorCode error_code) {
  // If we get an error, then error code must not be blank, and vice versa.
  DCHECK(results == UPGRADE_ERROR ? error_code != GOOGLE_UPDATE_NO_ERROR :
                                    error_code == GOOGLE_UPDATE_NO_ERROR);
  if (listener_)
    listener_->OnReportResults(results, error_code, version_available_);
}

