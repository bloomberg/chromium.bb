// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/risk/fingerprint.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/port.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/testing_pref_service.h"
#include "chrome/browser/autofill/risk/proto/fingerprint.pb.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebScreenInfo.h"
#include "ui/gfx/rect.h"

namespace autofill {
namespace risk {

const int64 kGaiaId = GG_INT64_C(99194853094755497);
const char kCharset[] = "UTF-8";
const char kAcceptLanguages[] = "en-US,en";
const int kScreenColorDepth = 53;

class AutofillRiskFingerprintTest : public InProcessBrowserTest {
 public:
  AutofillRiskFingerprintTest()
      : kWindowBounds(2, 3, 5, 7),
        kContentBounds(11, 13, 17, 37),
        kScreenBounds(0, 0, 101, 71),
        kAvailableScreenBounds(0, 11, 101, 60),
        kUnavailableScreenBounds(0, 0, 101, 11),
        message_loop_(MessageLoop::TYPE_UI) {}

  void GetFingerprintTestCallback(scoped_ptr<Fingerprint> fingerprint) {
    // TODO(isherman): Investigating http://crbug.com/174296
    LOG(WARNING) << "Callback called.";

    // Verify that all fields Chrome can fill have been filled.
    ASSERT_TRUE(fingerprint->has_machine_characteristics());
    const Fingerprint_MachineCharacteristics& machine =
        fingerprint->machine_characteristics();
    ASSERT_TRUE(machine.has_operating_system_build());
    ASSERT_TRUE(machine.has_browser_install_time_hours());
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
    ASSERT_TRUE(machine.has_screen_color_depth());
    ASSERT_TRUE(machine.has_unavailable_screen_size());
    ASSERT_TRUE(machine.unavailable_screen_size().has_width());
    ASSERT_TRUE(machine.unavailable_screen_size().has_height());
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
    EXPECT_EQ(kScreenColorDepth, machine.screen_color_depth());
    EXPECT_EQ(kUnavailableScreenBounds.width(),
              machine.unavailable_screen_size().width());
    EXPECT_EQ(kUnavailableScreenBounds.height(),
              machine.unavailable_screen_size().height());
    EXPECT_EQ(kContentBounds.width(),
              transient_state.inner_window_size().width());
    EXPECT_EQ(kContentBounds.height(),
              transient_state.inner_window_size().height());
    EXPECT_EQ(kWindowBounds.width(),
              transient_state.outer_window_size().width());
    EXPECT_EQ(kWindowBounds.height(),
              transient_state.outer_window_size().height());
    EXPECT_EQ(kGaiaId, fingerprint->metadata().gaia_id());

    // TODO(isherman): Investigating http://crbug.com/174296
    LOG(WARNING) << "Stopping the message loop.";
    message_loop_.Quit();
  }

 protected:
  const gfx::Rect kWindowBounds;
  const gfx::Rect kContentBounds;
  const gfx::Rect kScreenBounds;
  const gfx::Rect kAvailableScreenBounds;
  const gfx::Rect kUnavailableScreenBounds;
  MessageLoop message_loop_;
};

// This test is flaky on Windows. See http://crbug.com/178356.
#if defined(OS_WIN)
#define MAYBE_GetFingerprint DISABLED_GetFingerprint
#else
#define MAYBE_GetFingerprint GetFingerprint
#endif
// Test that getting a fingerprint works on some basic level.
IN_PROC_BROWSER_TEST_F(AutofillRiskFingerprintTest, MAYBE_GetFingerprint) {
  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterStringPref(prefs::kDefaultCharset, kCharset);
  prefs.registry()->RegisterStringPref(prefs::kAcceptLanguages,
                                       kAcceptLanguages);

  WebKit::WebScreenInfo screen_info;
  screen_info.depth = kScreenColorDepth;
  screen_info.rect = WebKit::WebRect(kScreenBounds);
  screen_info.availableRect = WebKit::WebRect(kAvailableScreenBounds);

  // TODO(isherman): Investigating http://crbug.com/174296
  LOG(WARNING) << "Loading fingerprint.";
  internal::GetFingerprintInternal(
      kGaiaId, kWindowBounds, kContentBounds, screen_info, prefs,
      base::Bind(&AutofillRiskFingerprintTest::GetFingerprintTestCallback,
                 base::Unretained(this)));

  // Wait for the callback to be called.
  // TODO(isherman): Investigating http://crbug.com/174296
  LOG(WARNING) << "Waiting for the callback to be called.";
  message_loop_.Run();
}

}  // namespace risk
}  // namespace autofill
