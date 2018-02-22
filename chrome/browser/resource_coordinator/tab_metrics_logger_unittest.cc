// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_metrics_logger.h"

#include "chrome/browser/resource_coordinator/tab_metrics_event.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using metrics::TabMetricsEvent;

// Sanity checks for functions in TabMetricsLogger.
// See TabActivityWatcherTest for more thorough tab usage UKM tests.

// Tests that protocol schemes are mapped to the correct enumerators.
TEST(TabMetricsLoggerTest, Schemes) {
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_BITCOIN,
            TabMetricsLogger::GetSchemeValueFromString("bitcoin"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_GEO,
            TabMetricsLogger::GetSchemeValueFromString("geo"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_IM,
            TabMetricsLogger::GetSchemeValueFromString("im"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_IRC,
            TabMetricsLogger::GetSchemeValueFromString("irc"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_IRCS,
            TabMetricsLogger::GetSchemeValueFromString("ircs"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_MAGNET,
            TabMetricsLogger::GetSchemeValueFromString("magnet"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_MAILTO,
            TabMetricsLogger::GetSchemeValueFromString("mailto"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_MMS,
            TabMetricsLogger::GetSchemeValueFromString("mms"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_NEWS,
            TabMetricsLogger::GetSchemeValueFromString("news"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_NNTP,
            TabMetricsLogger::GetSchemeValueFromString("nntp"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_OPENPGP4FPR,
            TabMetricsLogger::GetSchemeValueFromString("openpgp4fpr"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_SIP,
            TabMetricsLogger::GetSchemeValueFromString("sip"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_SMS,
            TabMetricsLogger::GetSchemeValueFromString("sms"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_SMSTO,
            TabMetricsLogger::GetSchemeValueFromString("smsto"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_SSH,
            TabMetricsLogger::GetSchemeValueFromString("ssh"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_TEL,
            TabMetricsLogger::GetSchemeValueFromString("tel"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_URN,
            TabMetricsLogger::GetSchemeValueFromString("urn"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_WEBCAL,
            TabMetricsLogger::GetSchemeValueFromString("webcal"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_WTAI,
            TabMetricsLogger::GetSchemeValueFromString("wtai"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_XMPP,
            TabMetricsLogger::GetSchemeValueFromString("xmpp"));

  static_assert(
      TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_LAST ==
              TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_XMPP &&
          TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_LAST == 20,
      "This test and the scheme list in TabMetricsLoggerImpl must be updated "
      "when new protocol handlers are added.");
}

// Tests non-whitelisted protocol schemes.
TEST(TabMetricsLoggerTest, NonWhitelistedSchemes) {
  // Native (non-web-based) handler.
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_OTHER,
            TabMetricsLogger::GetSchemeValueFromString("foo"));

  // Custom ("web+") protocol handlers.
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_OTHER,
            TabMetricsLogger::GetSchemeValueFromString("web+foo"));
  // "mailto" after the web+ prefix doesn't trigger any special handling.
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_OTHER,
            TabMetricsLogger::GetSchemeValueFromString("web+mailto"));

  // Nonsense protocol handlers.
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_OTHER,
            TabMetricsLogger::GetSchemeValueFromString(""));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_OTHER,
            TabMetricsLogger::GetSchemeValueFromString("mailto-xyz"));
}
