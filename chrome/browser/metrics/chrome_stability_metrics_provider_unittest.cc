// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_stability_metrics_provider.h"

#include "base/macros.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/metrics/proto/system_profile.pb.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/process_type.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/features/features.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/process_map.h"
#endif

namespace {

class ChromeStabilityMetricsProviderTest : public testing::Test {
 protected:
  ChromeStabilityMetricsProviderTest() : prefs_(new TestingPrefServiceSimple) {
    metrics::StabilityMetricsHelper::RegisterPrefs(prefs()->registry());
  }

  TestingPrefServiceSimple* prefs() { return prefs_.get(); }

 private:
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
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
  std::unique_ptr<TestingProfileManager> profile_manager(
      new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
  EXPECT_TRUE(profile_manager->SetUp());

  // Owned by profile_manager.
  TestingProfile* profile(
      profile_manager->CreateTestingProfile("StabilityTestProfile"));

  std::unique_ptr<content::MockRenderProcessHostFactory> rph_factory(
      new content::MockRenderProcessHostFactory());
  scoped_refptr<content::SiteInstance> site_instance(
      content::SiteInstance::Create(profile));

  // Owned by rph_factory.
  content::RenderProcessHost* host(
      rph_factory->CreateRenderProcessHost(profile));

  // Crash and abnormal termination should increment renderer crash count.
  content::RenderProcessHost::RendererClosedDetails crash_details(
      base::TERMINATION_STATUS_PROCESS_CRASHED, 1);
  provider.Observe(
      content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
      content::Source<content::RenderProcessHost>(host),
      content::Details<content::RenderProcessHost::RendererClosedDetails>(
          &crash_details));

  content::RenderProcessHost::RendererClosedDetails term_details(
      base::TERMINATION_STATUS_ABNORMAL_TERMINATION, 1);
  provider.Observe(
      content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
      content::Source<content::RenderProcessHost>(host),
      content::Details<content::RenderProcessHost::RendererClosedDetails>(
          &term_details));

  // Kill does not increment renderer crash count.
  content::RenderProcessHost::RendererClosedDetails kill_details(
      base::TERMINATION_STATUS_PROCESS_WAS_KILLED, 1);
  provider.Observe(
      content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
      content::Source<content::RenderProcessHost>(host),
      content::Details<content::RenderProcessHost::RendererClosedDetails>(
          &kill_details));

  // Failed launch increments failed launch count.
  content::RenderProcessHost::RendererClosedDetails failed_launch_details(
      base::TERMINATION_STATUS_LAUNCH_FAILED, 1);
  provider.Observe(
      content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
      content::Source<content::RenderProcessHost>(host),
      content::Details<content::RenderProcessHost::RendererClosedDetails>(
          &failed_launch_details));

  metrics::SystemProfileProto system_profile;

  // Call ProvideStabilityMetrics to check that it will force pending tasks to
  // be executed immediately.
  provider.ProvideStabilityMetrics(&system_profile);

  EXPECT_EQ(2, system_profile.stability().renderer_crash_count());
  EXPECT_EQ(1, system_profile.stability().renderer_failed_launch_count());
  EXPECT_EQ(0, system_profile.stability().extension_renderer_crash_count());

#if BUILDFLAG(ENABLE_EXTENSIONS)
  provider.ClearSavedStabilityMetrics();

  // Owned by rph_factory.
  content::RenderProcessHost* extension_host(
      rph_factory->CreateRenderProcessHost(profile));

  // Make the rph an extension rph.
  extensions::ProcessMap::Get(profile)
      ->Insert("1", extension_host->GetID(), site_instance->GetId());

  // Crash and abnormal termination should increment extension crash count.
  provider.Observe(
      content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
      content::Source<content::RenderProcessHost>(extension_host),
      content::Details<content::RenderProcessHost::RendererClosedDetails>(
          &crash_details));

  // Failed launch increments failed launch count.
  provider.Observe(
      content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
      content::Source<content::RenderProcessHost>(extension_host),
      content::Details<content::RenderProcessHost::RendererClosedDetails>(
          &failed_launch_details));

  system_profile.Clear();
  provider.ProvideStabilityMetrics(&system_profile);

  EXPECT_EQ(0, system_profile.stability().renderer_crash_count());
  EXPECT_EQ(1, system_profile.stability().extension_renderer_crash_count());
  EXPECT_EQ(
      1, system_profile.stability().extension_renderer_failed_launch_count());
#endif

  profile_manager->DeleteAllTestingProfiles();
}
