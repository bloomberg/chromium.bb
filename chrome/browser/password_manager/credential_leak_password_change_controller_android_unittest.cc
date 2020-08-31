// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/credential_leak_password_change_controller_android.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using password_manager::CreateLeakType;
using password_manager::IsReused;
using password_manager::IsSaved;
using password_manager::IsSyncing;
using password_manager::metrics_util::LeakDialogDismissalReason;

namespace {

// The On*Dialog() methods used by the tests below all invoke `delete this;`,
// thus there is no memory leak here.
CredentialLeakPasswordChangeControllerAndroid*
MakeController(IsSaved is_saved, IsReused is_reused, IsSyncing is_syncing) {
  return new CredentialLeakPasswordChangeControllerAndroid(
      CreateLeakType(is_saved, is_reused, is_syncing),
      GURL("https://example.com"), base::ASCIIToUTF16("test_username"),
      nullptr);
}

}  // namespace

TEST(CredentialLeakPasswordChangeControllerAndroidTest, ClickedCancel) {
  base::HistogramTester histogram_tester;

  MakeController(IsSaved(false), IsReused(true), IsSyncing(true))
      ->OnCancelDialog();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason",
      LeakDialogDismissalReason::kClickedClose, 1);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason.CheckupAndChange",
      LeakDialogDismissalReason::kClickedClose, 1);
}

TEST(CredentialLeakPasswordChangeControllerAndroidTest, ClickedOk) {
  base::HistogramTester histogram_tester;

  MakeController(IsSaved(false), IsReused(false), IsSyncing(false))
      ->OnAcceptDialog();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason",
      LeakDialogDismissalReason::kClickedOk, 1);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason.Change",
      LeakDialogDismissalReason::kClickedOk, 1);
}

TEST(CredentialLeakPasswordChangeControllerAndroidTest, ClickedCheckPasswords) {
  base::HistogramTester histogram_tester;

  MakeController(IsSaved(true), IsReused(true), IsSyncing(true))
      ->OnAcceptDialog();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason",
      LeakDialogDismissalReason::kClickedCheckPasswords, 1);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason.Checkup",
      LeakDialogDismissalReason::kClickedCheckPasswords, 1);
}

TEST(CredentialLeakPasswordChangeControllerAndroidTest, NoDirectInteraction) {
  base::HistogramTester histogram_tester;

  MakeController(IsSaved(false), IsReused(false), IsSyncing(false))
      ->OnCloseDialog();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason",
      LeakDialogDismissalReason::kNoDirectInteraction, 1);

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.LeakDetection.DialogDismissalReason.Change",
      LeakDialogDismissalReason::kNoDirectInteraction, 1);
}
