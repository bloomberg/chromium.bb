// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_upgrade_container_view.h"

#include "base/metrics/histogram_base.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/crostini/crostini_browser_test_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cicerone_client.h"
#include "chromeos/dbus/fake_concierge_client.h"
#include "components/crx_file/id_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/window/dialog_client_view.h"

const char kVmName[] = "vm_name";
const char kContainerName[] = "container_name";

chromeos::FakeCiceroneClient* GetFakeCiceroneClient() {
  return static_cast<chromeos::FakeCiceroneClient*>(
      chromeos::DBusThreadManager::Get()->GetCiceroneClient());
}

class CrostiniUpgradeContainerViewBrowserTest
    : public CrostiniDialogBrowserTest {
 public:
  CrostiniUpgradeContainerViewBrowserTest()
      : CrostiniDialogBrowserTest(true /*register_termina*/) {}

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    PrepareShowCrostiniUpgradeContainerView(
        browser()->profile(), crostini::CrostiniUISurface::kAppList);
    base::RunLoop().RunUntilIdle();
  }

  CrostiniUpgradeContainerView* ActiveView() {
    return CrostiniUpgradeContainerView::GetActiveViewForTesting();
  }

  bool HasAcceptButton() {
    return ActiveView()->GetDialogClientView()->ok_button() != nullptr;
  }

  bool HasCancelButton() {
    return ActiveView()->GetDialogClientView()->cancel_button() != nullptr;
  }

  void ExpectView() {
    base::RunLoop().RunUntilIdle();
    // A new Widget was created in ShowUi() or since the last VerifyUi().
    EXPECT_TRUE(VerifyUi());
    // There is one view, and it's ours.
    EXPECT_NE(nullptr, ActiveView());
  }

  void ExpectNoView() {
    base::RunLoop().RunUntilIdle();
    // No new Widget was created in ShowUi() or since the last VerifyUi().
    EXPECT_FALSE(VerifyUi());
    // Our view has really been deleted.
    EXPECT_EQ(nullptr, ActiveView());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CrostiniUpgradeContainerViewBrowserTest);
};

// Test the dialog is actually launched.
IN_PROC_BROWSER_TEST_F(CrostiniUpgradeContainerViewBrowserTest,
                       InvokeUi_default) {
  crostini::SetCrostiniUpgradeSkipDelayForTesting(true);
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(CrostiniUpgradeContainerViewBrowserTest, HitOK) {
  base::HistogramTester histogram_tester;
  crostini::SetCrostiniUpgradeSkipDelayForTesting(true);

  ShowUi("default");
  ExpectView();
  EXPECT_EQ(ui::DIALOG_BUTTON_OK, ActiveView()->GetDialogButtons());

  EXPECT_TRUE(HasAcceptButton());
  EXPECT_FALSE(HasCancelButton());

  ActiveView()->GetDialogClientView()->AcceptWindow();
  EXPECT_TRUE(ActiveView()->GetWidget()->IsClosed());
  ExpectNoView();

  histogram_tester.ExpectUniqueSample(
      "Crostini.UpgradeContainerSource",
      static_cast<base::HistogramBase::Sample>(
          crostini::CrostiniUISurface::kAppList),
      1);
}

IN_PROC_BROWSER_TEST_F(CrostiniUpgradeContainerViewBrowserTest,
                       StartLxdContainerNoUpgradeNeeded) {
  base::HistogramTester histogram_tester;
  crostini::SetCrostiniUpgradeSkipDelayForTesting(true);

  vm_tools::cicerone::StartLxdContainerResponse reply;
  reply.set_status(vm_tools::cicerone::StartLxdContainerResponse::STARTING);
  GetFakeCiceroneClient()->set_start_lxd_container_response(reply);

  crostini::CrostiniManager::GetForProfile(browser()->profile())
      ->StartLxdContainer(kVmName, kContainerName, base::DoNothing());
  ExpectNoView();
}

IN_PROC_BROWSER_TEST_F(CrostiniUpgradeContainerViewBrowserTest,
                       StartLxdContainerUpgradeNeeded) {
  base::HistogramTester histogram_tester;
  crostini::SetCrostiniUpgradeSkipDelayForTesting(true);

  vm_tools::cicerone::StartLxdContainerResponse reply;
  reply.set_status(vm_tools::cicerone::StartLxdContainerResponse::REMAPPING);
  GetFakeCiceroneClient()->set_start_lxd_container_response(reply);

  crostini::CrostiniManager::GetForProfile(browser()->profile())
      ->StartLxdContainer(kVmName, kContainerName, base::DoNothing());
  ExpectView();

  ActiveView()->GetDialogClientView()->AcceptWindow();
  EXPECT_TRUE(ActiveView()->GetWidget()->IsClosed());
  ExpectNoView();

  histogram_tester.ExpectUniqueSample(
      "Crostini.UpgradeContainerSource",
      static_cast<base::HistogramBase::Sample>(
          crostini::CrostiniUISurface::kAppList),
      1);
}
