// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_metrics_logger_impl.h"

#include "chrome/browser/ui/tabs/tab_metrics_event.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using metrics::TabMetricsEvent;

// Sanity checks for TabMetricsLoggerImpl.
// See TabActivityWatcherTest for more thorough tab usage UKM tests.

// Tests that protocol schemes are mapped to the correct enumerators.
TEST(TabMetricsLoggerImplTest, Schemes) {
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_BITCOIN,
            TabMetricsLoggerImpl::GetSchemeValueFromString("bitcoin"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_GEO,
            TabMetricsLoggerImpl::GetSchemeValueFromString("geo"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_IM,
            TabMetricsLoggerImpl::GetSchemeValueFromString("im"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_IRC,
            TabMetricsLoggerImpl::GetSchemeValueFromString("irc"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_IRCS,
            TabMetricsLoggerImpl::GetSchemeValueFromString("ircs"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_MAGNET,
            TabMetricsLoggerImpl::GetSchemeValueFromString("magnet"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_MAILTO,
            TabMetricsLoggerImpl::GetSchemeValueFromString("mailto"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_MMS,
            TabMetricsLoggerImpl::GetSchemeValueFromString("mms"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_NEWS,
            TabMetricsLoggerImpl::GetSchemeValueFromString("news"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_NNTP,
            TabMetricsLoggerImpl::GetSchemeValueFromString("nntp"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_OPENPGP4FPR,
            TabMetricsLoggerImpl::GetSchemeValueFromString("openpgp4fpr"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_SIP,
            TabMetricsLoggerImpl::GetSchemeValueFromString("sip"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_SMS,
            TabMetricsLoggerImpl::GetSchemeValueFromString("sms"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_SMSTO,
            TabMetricsLoggerImpl::GetSchemeValueFromString("smsto"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_SSH,
            TabMetricsLoggerImpl::GetSchemeValueFromString("ssh"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_TEL,
            TabMetricsLoggerImpl::GetSchemeValueFromString("tel"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_URN,
            TabMetricsLoggerImpl::GetSchemeValueFromString("urn"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_WEBCAL,
            TabMetricsLoggerImpl::GetSchemeValueFromString("webcal"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_WTAI,
            TabMetricsLoggerImpl::GetSchemeValueFromString("wtai"));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_XMPP,
            TabMetricsLoggerImpl::GetSchemeValueFromString("xmpp"));

  static_assert(
      TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_LAST ==
              TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_XMPP &&
          TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_LAST == 20,
      "This test and the scheme list in TabMetricsLoggerImpl must be updated "
      "when new protocol handlers are added.");
}

// Tests non-whitelisted protocol schemes.
TEST(TabMetricsLoggerImplTest, NonWhitelistedSchemes) {
  // Native (non-web-based) handler.
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_OTHER,
            TabMetricsLoggerImpl::GetSchemeValueFromString("foo"));

  // Custom ("web+") protocol handlers.
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_OTHER,
            TabMetricsLoggerImpl::GetSchemeValueFromString("web+foo"));
  // "mailto" after the web+ prefix doesn't trigger any special handling.
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_OTHER,
            TabMetricsLoggerImpl::GetSchemeValueFromString("web+mailto"));

  // Nonsense protocol handlers.
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_OTHER,
            TabMetricsLoggerImpl::GetSchemeValueFromString(""));
  EXPECT_EQ(TabMetricsEvent::PROTOCOL_HANDLER_SCHEME_OTHER,
            TabMetricsLoggerImpl::GetSchemeValueFromString("mailto-xyz"));
}
