// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <ostream>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/chromeos/arc/arc_support_host.h"
#include "chrome/browser/chromeos/arc/extensions/fake_arc_support.h"
#include "chrome/browser/chromeos/arc/optin/arc_terms_of_service_default_negotiator.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

class ArcTermsOfServiceDefaultNegotiatorTest : public testing::Test {
 public:
  ArcTermsOfServiceDefaultNegotiatorTest() = default;
  ~ArcTermsOfServiceDefaultNegotiatorTest() override = default;

  void SetUp() override {
    user_manager_enabler_ =
        base::MakeUnique<chromeos::ScopedUserManagerEnabler>(
            new chromeos::FakeChromeUserManager());

    profile_ = base::MakeUnique<TestingProfile>();

    support_host_ = base::MakeUnique<ArcSupportHost>(profile_.get());
    fake_arc_support_ = base::MakeUnique<FakeArcSupport>(support_host_.get());
    negotiator_ = base::MakeUnique<ArcTermsOfServiceDefaultNegotiator>(
        profile_->GetPrefs(), support_host());
  }

  void TearDown() override {
    negotiator_.reset();
    fake_arc_support_.reset();
    support_host_.reset();
    profile_.reset();
    user_manager_enabler_.reset();
  }

  TestingProfile* profile() { return profile_.get(); }
  ArcSupportHost* support_host() { return support_host_.get(); }
  FakeArcSupport* fake_arc_support() { return fake_arc_support_.get(); }
  ArcTermsOfServiceNegotiator* negotiator() { return negotiator_.get(); }

 private:
  // Fake as if the current testing thread is UI thread.
  content::TestBrowserThreadBundle bundle_;

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<chromeos::ScopedUserManagerEnabler> user_manager_enabler_;
  std::unique_ptr<ArcSupportHost> support_host_;
  std::unique_ptr<FakeArcSupport> fake_arc_support_;
  std::unique_ptr<ArcTermsOfServiceNegotiator> negotiator_;

  DISALLOW_COPY_AND_ASSIGN(ArcTermsOfServiceDefaultNegotiatorTest);
};

namespace {

enum class Status {
  PENDING,
  ACCEPTED,
  CANCELLED,
};

// For better logging.
std::ostream& operator<<(std::ostream& os, Status status) {
  switch (status) {
    case Status::PENDING:
      return os << "PENDING";
    case Status::ACCEPTED:
      return os << "ACCEPTED";
    case Status::CANCELLED:
      return os << "CANCELLED";
  }

  NOTREACHED();
  return os;
}

ArcTermsOfServiceNegotiator::NegotiationCallback UpdateStatusCallback(
    Status* status) {
  return base::Bind(
      [](Status* status, bool accepted) {
        *status = accepted ? Status::ACCEPTED : Status::CANCELLED;
      },
      status);
}

}  // namespace

TEST_F(ArcTermsOfServiceDefaultNegotiatorTest, Accept) {
  // Show Terms of service page.
  Status status = Status::PENDING;
  negotiator()->StartNegotiation(UpdateStatusCallback(&status));

  // TERMS page should be shown.
  EXPECT_EQ(status, Status::PENDING);
  EXPECT_EQ(fake_arc_support()->ui_page(), ArcSupportHost::UIPage::TERMS);

  // By default, the preference related checkboxes are checked, despite that
  // the preferences default to false.
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcBackupRestoreEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcLocationServiceEnabled));
  EXPECT_TRUE(fake_arc_support()->backup_and_restore_mode());
  EXPECT_TRUE(fake_arc_support()->location_service_mode());

  // The preferences are assigned to the managed false value, and the
  // corresponding checkboxes are unchecked.
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcBackupRestoreEnabled, new base::Value(false));
  EXPECT_FALSE(fake_arc_support()->backup_and_restore_mode());
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kArcLocationServiceEnabled, new base::Value(false));
  EXPECT_FALSE(fake_arc_support()->location_service_mode());

  // The managed preference values are removed, and the corresponding checkboxes
  // are checked again.
  profile()->GetTestingPrefService()->RemoveManagedPref(
      prefs::kArcBackupRestoreEnabled);
  EXPECT_TRUE(fake_arc_support()->backup_and_restore_mode());
  profile()->GetTestingPrefService()->RemoveManagedPref(
      prefs::kArcLocationServiceEnabled);
  EXPECT_TRUE(fake_arc_support()->location_service_mode());

  // Make sure preference values are not yet updated.
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcBackupRestoreEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcLocationServiceEnabled));

  // Click the "AGREE" button so that the callback should be invoked
  // with |agreed| = true.
  fake_arc_support()->ClickAgreeButton();
  EXPECT_EQ(status, Status::ACCEPTED);

  // Make sure preference values are now updated.
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcBackupRestoreEnabled));
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcLocationServiceEnabled));
}

TEST_F(ArcTermsOfServiceDefaultNegotiatorTest, AcceptWithUnchecked) {
  // Show Terms of service page.
  Status status = Status::PENDING;
  negotiator()->StartNegotiation(UpdateStatusCallback(&status));

  // TERMS page should be shown.
  EXPECT_EQ(status, Status::PENDING);
  EXPECT_EQ(fake_arc_support()->ui_page(), ArcSupportHost::UIPage::TERMS);

  // Override the preferences from the default values to true.
  profile()->GetPrefs()->SetBoolean(prefs::kArcBackupRestoreEnabled, true);
  profile()->GetPrefs()->SetBoolean(prefs::kArcLocationServiceEnabled, true);

  // Uncheck the preference related checkboxes.
  fake_arc_support()->set_backup_and_restore_mode(false);
  fake_arc_support()->set_location_service_mode(false);

  // Make sure preference values are not yet updated.
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcBackupRestoreEnabled));
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcLocationServiceEnabled));

  // Click the "AGREE" button so that the callback should be invoked
  // with |agreed| = true.
  fake_arc_support()->ClickAgreeButton();
  EXPECT_EQ(status, Status::ACCEPTED);

  // Make sure preference values are now updated.
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcBackupRestoreEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcLocationServiceEnabled));
}

TEST_F(ArcTermsOfServiceDefaultNegotiatorTest, Cancel) {
  // Show Terms of service page.
  Status status = Status::PENDING;
  negotiator()->StartNegotiation(UpdateStatusCallback(&status));

  // TERMS page should be shown.
  EXPECT_EQ(status, Status::PENDING);
  EXPECT_EQ(fake_arc_support()->ui_page(), ArcSupportHost::UIPage::TERMS);

  // Check the preference related checkbox.
  fake_arc_support()->set_metrics_mode(true);
  fake_arc_support()->set_backup_and_restore_mode(true);
  fake_arc_support()->set_location_service_mode(true);

  // Make sure preference values are not yet updated.
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcBackupRestoreEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcLocationServiceEnabled));

  // Clicking "CANCEL" button closes the window.
  fake_arc_support()->Close();
  EXPECT_EQ(status, Status::CANCELLED);

  // Make sure preference checkbox values are discarded.
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcBackupRestoreEnabled));
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcLocationServiceEnabled));
}

TEST_F(ArcTermsOfServiceDefaultNegotiatorTest, Retry) {
  // Show Terms of service page.
  Status status = Status::PENDING;
  negotiator()->StartNegotiation(UpdateStatusCallback(&status));

  // TERMS page should be shown.
  EXPECT_EQ(status, Status::PENDING);
  EXPECT_EQ(fake_arc_support()->ui_page(), ArcSupportHost::UIPage::TERMS);

  // Switch to error page.
  support_host()->ShowError(ArcSupportHost::Error::SIGN_IN_NETWORK_ERROR,
                            false);

  // The callback should not be called yet.
  EXPECT_EQ(status, Status::PENDING);
  EXPECT_EQ(fake_arc_support()->ui_page(), ArcSupportHost::UIPage::ERROR);

  // Click RETRY button on the page, then Terms of service page should be
  // re-shown.
  fake_arc_support()->ClickRetryButton();
  EXPECT_EQ(status, Status::PENDING);
  EXPECT_EQ(fake_arc_support()->ui_page(), ArcSupportHost::UIPage::TERMS);
}

}  // namespace arc
