// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/risk/fingerprint.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/prefs/public/pref_service_base.h"
#include "chrome/browser/autofill/risk/proto/fingerprint.pb.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect.h"

namespace autofill {
namespace risk {

const int64 kGaiaId = 99194853094755497;
const char kCharset[] = "UTF-8";
const char kAcceptLanguages[] = "en-US,en";

class AutofillRiskFingerprintTest : public InProcessBrowserTest {
 public:
  AutofillRiskFingerprintTest()
      : kWindowBounds(2, 3, 5, 7),
        kContentBounds(11, 13, 17, 37),
        message_loop_(MessageLoop::TYPE_UI) {}

  void GetFingerprintTestCallback(scoped_ptr<Fingerprint> fingerprint) {
    // Verify that all fields Chrome can fill have been filled.
    ASSERT_TRUE(fingerprint->has_machine_characteristics());
    const Fingerprint_MachineCharacteristics& machine =
        fingerprint->machine_characteristics();
    ASSERT_TRUE(machine.has_operating_system_build());
    ASSERT_GT(machine.font_size(), 0);
    ASSERT_GT(machine.plugin_size(), 0);
    ASSERT_TRUE(machine.has_utc_offset_ms());
    ASSERT_TRUE(machine.has_browser_language());
    ASSERT_GT(machine.requested_language_size(), 0);
    ASSERT_TRUE(machine.has_charset());
    ASSERT_TRUE(machine.has_screen_count());
    ASSERT_TRUE(machine.has_screen_size());
    ASSERT_TRUE(machine.screen_size().has_width());
    ASSERT_TRUE(machine.screen_size().has_height());
    ASSERT_TRUE(machine.has_user_agent());
    ASSERT_TRUE(machine.has_cpu());
    ASSERT_TRUE(machine.cpu().has_vendor_name());
    ASSERT_TRUE(machine.cpu().has_brand());
    ASSERT_TRUE(machine.has_ram());
    ASSERT_TRUE(machine.has_graphics_card());
    ASSERT_TRUE(machine.graphics_card().has_vendor_id());
    ASSERT_TRUE(machine.graphics_card().has_device_id());
    ASSERT_TRUE(machine.has_browser_build());

    ASSERT_TRUE(fingerprint->has_transient_state());
    const Fingerprint_TransientState& transient_state =
        fingerprint->transient_state();
    ASSERT_TRUE(transient_state.has_inner_window_size());
    ASSERT_TRUE(transient_state.has_outer_window_size());
    ASSERT_TRUE(transient_state.inner_window_size().has_width());
    ASSERT_TRUE(transient_state.inner_window_size().has_height());
    ASSERT_TRUE(transient_state.outer_window_size().has_width());
    ASSERT_TRUE(transient_state.outer_window_size().has_height());

    ASSERT_TRUE(fingerprint->has_metadata());
    ASSERT_TRUE(fingerprint->metadata().has_timestamp_ms());
    ASSERT_TRUE(fingerprint->metadata().has_gaia_id());
    ASSERT_TRUE(fingerprint->metadata().has_fingerprinter_version());

    // Some values have exact known (mocked out) values:
    ASSERT_EQ(2, machine.requested_language_size());
    EXPECT_EQ("en-US", machine.requested_language(0));
    EXPECT_EQ("en", machine.requested_language(1));
    EXPECT_EQ(kCharset, machine.charset());
    EXPECT_EQ(kContentBounds.width(),
              transient_state.inner_window_size().width());
    EXPECT_EQ(kContentBounds.height(),
              transient_state.inner_window_size().height());
    EXPECT_EQ(kWindowBounds.width(),
              transient_state.outer_window_size().width());
    EXPECT_EQ(kWindowBounds.height(),
              transient_state.outer_window_size().height());
    EXPECT_EQ(kGaiaId, fingerprint->metadata().gaia_id());

    message_loop_.Quit();
  }

 protected:
  const gfx::Rect kWindowBounds;
  const gfx::Rect kContentBounds;
  MessageLoop message_loop_;
};

// Test that getting a fingerprint works on some basic level.
IN_PROC_BROWSER_TEST_F(AutofillRiskFingerprintTest, GetFingerprint) {
  TestingPrefServiceSimple prefs;
  prefs.RegisterStringPref(prefs::kDefaultCharset, kCharset);
  prefs.RegisterStringPref(prefs::kAcceptLanguages, kAcceptLanguages);

  GetFingerprint(
      kGaiaId, kWindowBounds, kContentBounds, prefs,
      base::Bind(&AutofillRiskFingerprintTest::GetFingerprintTestCallback,
                 base::Unretained(this)));

  // Wait for the callback to be called.
  message_loop_.Run();
}

}  // namespace risk
}  // namespace autofill
