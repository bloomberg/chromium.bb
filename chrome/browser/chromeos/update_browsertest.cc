// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/mock_update_library.h"
#include "chrome/browser/chromeos/update_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/automation/dom_element_proxy.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"

using ::testing::AnyNumber;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Exactly;

namespace {

typedef ObserverList<chromeos::UpdateLibrary::Observer> Observers;

void CallObservers(chromeos::MockUpdateLibrary* lib,
                   Observers* observers,
                   const chromeos::UpdateLibrary::Status& x) {
  EXPECT_CALL(*lib, status())
    .Times(AnyNumber())
    .WillRepeatedly((ReturnRef(x)))
    .RetiresOnSaturation();
  FOR_EACH_OBSERVER(chromeos::UpdateLibrary::Observer, *observers,
                    UpdateStatusChanged(lib));
}

void FireSuccessSequence(chromeos::MockUpdateLibrary* lib,
                         Observers* observer) {
  chromeos::UpdateLibrary::Status status;

  status.status = chromeos::UPDATE_STATUS_IDLE;
  CallObservers(lib, observer, status);

  status.status = chromeos::UPDATE_STATUS_CHECKING_FOR_UPDATE;
  CallObservers(lib, observer, status);

  status.status = chromeos::UPDATE_STATUS_UPDATE_AVAILABLE;
  CallObservers(lib, observer, status);

  status.status = chromeos::UPDATE_STATUS_DOWNLOADING;
  status.download_progress = 10;
  CallObservers(lib, observer, status);

  status.status = chromeos::UPDATE_STATUS_DOWNLOADING;
  status.download_progress = 50;
  CallObservers(lib, observer, status);

  status.status = chromeos::UPDATE_STATUS_DOWNLOADING;
  status.download_progress = 90;
  CallObservers(lib, observer, status);

  status.status = chromeos::UPDATE_STATUS_VERIFYING;
  CallObservers(lib, observer, status);

  status.status = chromeos::UPDATE_STATUS_FINALIZING;
  CallObservers(lib, observer, status);

  status.status = chromeos::UPDATE_STATUS_UPDATED_NEED_REBOOT;
  CallObservers(lib, observer, status);
}

void FireFailureSequence(chromeos::MockUpdateLibrary* lib,
                         Observers* observer) {
  chromeos::UpdateLibrary::Status status;

  status.status = chromeos::UPDATE_STATUS_IDLE;
  CallObservers(lib, observer, status);

  status.status = chromeos::UPDATE_STATUS_CHECKING_FOR_UPDATE;
  CallObservers(lib, observer, status);

  status.status = chromeos::UPDATE_STATUS_UPDATE_AVAILABLE;
  CallObservers(lib, observer, status);

  status.status = chromeos::UPDATE_STATUS_DOWNLOADING;
  status.download_progress = 10;
  CallObservers(lib, observer, status);

  status.status = chromeos::UPDATE_STATUS_ERROR;
  status.download_progress = 10;
  CallObservers(lib, observer, status);
}

class UpdateBrowserTest : public InProcessBrowserTest {
 public:
  UpdateBrowserTest() {}
};

IN_PROC_BROWSER_TEST_F(UpdateBrowserTest, Notifications) {
  scoped_ptr<chromeos::MockUpdateLibrary> lib(
      new chromeos::MockUpdateLibrary());

  Observers observers;

  EXPECT_CALL(*lib, AddObserver(_))
      .WillRepeatedly(Invoke(&observers,
                             &Observers::AddObserver));

  chromeos::UpdateObserver* observe =
      new chromeos::UpdateObserver(browser()->profile());
  lib->AddObserver(observe);

  FireSuccessSequence(lib.get(), &observers);
  FireFailureSequence(lib.get(), &observers);
}

}  // namespace
