// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_stability_metrics_provider.h"

#include "base/basictypes.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/prefs/testing_pref_service.h"
#include "components/metrics/proto/system_profile.pb.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/process_type.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/extensions_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Must be subclass of ExtensionsTest - see https://crbug.com/395820
class ChromeStabilityMetricsProviderTest : public extensions::ExtensionsTest {
 protected:
  ChromeStabilityMetricsProviderTest() : prefs_(new TestingPrefServiceSimple) {
    ChromeStabilityMetricsProvider::RegisterPrefs(prefs()->registry());
  }

  TestingPrefServiceSimple* prefs() { return prefs_.get(); }

 private:
  scoped_ptr<TestingPrefServiceSimple> prefs_;
  content::TestBrowserThreadBundle thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(ChromeStabilityMetricsProviderTest);
};

}  // namespace

TEST_F(ChromeStabilityMetricsProviderTest, BrowserChildProcessObserver) {
  ChromeStabilityMetricsProvider provider(prefs());

  content::ChildProcessData child_process_data(content::PROCESS_TYPE_RENDERER);
  provider.BrowserChildProcessCrashed(child_process_data, 1);
  provider.BrowserChildProcessCrashed(child_process_data, 1);

  // Call ProvideStabilityMetrics to check that it will force pending tasks to
  // be executed immediately.
  metrics::SystemProfileProto system_profile;

  provider.ProvideStabilityMetrics(&system_profile);

  // Check current number of instances created.
  const metrics::SystemProfileProto_Stability& stability =
      system_profile.stability();

  EXPECT_EQ(2, stability.child_process_crash_count());
}

TEST_F(ChromeStabilityMetricsProviderTest, NotificationObserver) {
  ChromeStabilityMetricsProvider provider(prefs());
  content::MockRenderProcessHost host(browser_context());

  // Crash and abnormal termination should increment renderer crash count.
  content::RenderProcessHost::RendererClosedDetails crash_details(
      base::TERMINATION_STATUS_PROCESS_CRASHED, 1);
  provider.Observe(
      content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
      content::Source<content::RenderProcessHost>(&host),
      content::Details<content::RenderProcessHost::RendererClosedDetails>(
          &crash_details));

  content::RenderProcessHost::RendererClosedDetails term_details(
      base::TERMINATION_STATUS_ABNORMAL_TERMINATION, 1);
  provider.Observe(
      content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
      content::Source<content::RenderProcessHost>(&host),
      content::Details<content::RenderProcessHost::RendererClosedDetails>(
          &term_details));

  // Kill does not increment renderer crash count.
  content::RenderProcessHost::RendererClosedDetails kill_details(
      base::TERMINATION_STATUS_PROCESS_WAS_KILLED, 1);
  provider.Observe(
      content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
      content::Source<content::RenderProcessHost>(&host),
      content::Details<content::RenderProcessHost::RendererClosedDetails>(
          &kill_details));

  metrics::SystemProfileProto system_profile;

  // Call ProvideStabilityMetrics to check that it will force pending tasks to
  // be executed immediately.
  provider.ProvideStabilityMetrics(&system_profile);

  const metrics::SystemProfileProto_Stability& stability =
      system_profile.stability();

  EXPECT_EQ(2, stability.renderer_crash_count());
}
