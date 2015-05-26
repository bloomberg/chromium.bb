// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/component_updater/chrome_component_updater_configurator.h"
#include "components/component_updater/component_updater_switches.h"
#include "components/update_client/configurator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace component_updater {

TEST(ChromeComponentUpdaterConfiguratorTest, TestDisablePings) {
  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  cmdline->AppendSwitchASCII(switches::kComponentUpdater, "disable-pings");

  const auto config(MakeChromeComponentUpdaterConfigurator(cmdline, NULL));

  const std::vector<GURL> pingUrls = config->PingUrl();
  EXPECT_TRUE(pingUrls.empty());
}

TEST(ChromeComponentUpdaterConfiguratorTest, TestFastUpdate) {
  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  cmdline->AppendSwitchASCII(switches::kComponentUpdater, "fast-update");

  const auto config(MakeChromeComponentUpdaterConfigurator(cmdline, NULL));

  ASSERT_EQ(10, config->InitialDelay());
}

TEST(ChromeComponentUpdaterConfiguratorTest, TestOverrideUrl) {
  const char overrideUrl[] = "http://0.0.0.0/";

  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  std::string val = "url-source";
  val.append("=");
  val.append(overrideUrl);
  cmdline->AppendSwitchASCII(switches::kComponentUpdater, val.c_str());

  const auto config(MakeChromeComponentUpdaterConfigurator(cmdline, NULL));

  const std::vector<GURL> urls = config->UpdateUrl();

  ASSERT_EQ(1U, urls.size());
  ASSERT_EQ(overrideUrl, urls.at(0).possibly_invalid_spec());
}

TEST(ChromeComponentUpdaterConfiguratorTest, TestSwitchRequestParam) {
  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  cmdline->AppendSwitchASCII(switches::kComponentUpdater, "test-request");

  const auto config(MakeChromeComponentUpdaterConfigurator(cmdline, NULL));

  EXPECT_FALSE(config->ExtraRequestParams().empty());
}

}  // namespace component_updater
