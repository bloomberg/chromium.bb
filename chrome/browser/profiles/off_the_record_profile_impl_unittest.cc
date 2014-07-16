// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/off_the_record_profile_impl.h"

#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/run_loop.h"
#include "chrome/browser/net/ssl_config_service_manager.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_io_thread_state.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/common/page_zoom.h"
#include "net/dns/mock_host_resolver.h"

using content::HostZoomMap;

namespace {

class TestingProfileWithHostZoomMap : public TestingProfile {
 public:
  TestingProfileWithHostZoomMap() {
    zoom_subscription_ =
        HostZoomMap::GetForBrowserContext(this)->AddZoomLevelChangedCallback(
            base::Bind(&TestingProfileWithHostZoomMap::OnZoomLevelChanged,
                        base::Unretained(this)));
  }

  virtual ~TestingProfileWithHostZoomMap() {}

  virtual Profile* GetOffTheRecordProfile() OVERRIDE {
    if (!off_the_record_profile_)
      off_the_record_profile_.reset(CreateOffTheRecordProfile());
    return off_the_record_profile_.get();
  }

  virtual PrefService* GetOffTheRecordPrefs() OVERRIDE {
    return GetPrefs();
  }

 private:
  void OnZoomLevelChanged(const HostZoomMap::ZoomLevelChange& change) {

    if (change.mode != HostZoomMap::ZOOM_CHANGED_FOR_HOST)
      return;

    HostZoomMap* host_zoom_map = HostZoomMap::GetForBrowserContext(this);

    double level = change.zoom_level;
    DictionaryPrefUpdate update(prefs_.get(), prefs::kPerHostZoomLevels);
    base::DictionaryValue* host_zoom_dictionary = update.Get();
    if (content::ZoomValuesEqual(level, host_zoom_map->GetDefaultZoomLevel())) {
      host_zoom_dictionary->RemoveWithoutPathExpansion(change.host, NULL);
    } else {
      host_zoom_dictionary->SetWithoutPathExpansion(
          change.host, new base::FundamentalValue(level));
    }
  }

  scoped_ptr<Profile> off_the_record_profile_;
  scoped_ptr<SSLConfigServiceManager> ssl_config_service_manager_;

  scoped_ptr<HostZoomMap::Subscription> zoom_subscription_;

  DISALLOW_COPY_AND_ASSIGN(TestingProfileWithHostZoomMap);
};

}  // namespace

// We need to have a BrowserProcess in g_browser_process variable, since
// OffTheRecordProfileImpl ctor uses it in
// ProfileIOData::InitializeProfileParams.
class OffTheRecordProfileImplTest : public BrowserWithTestWindowTest {
 protected:
  OffTheRecordProfileImplTest() {}

  virtual ~OffTheRecordProfileImplTest() {}

  virtual void SetUp() OVERRIDE {
    profile_manager_.reset(new TestingProfileManager(browser_process()));
    ASSERT_TRUE(profile_manager_->SetUp());

    testing_io_thread_state_.reset(new chrome::TestingIOThreadState());
    testing_io_thread_state_->io_thread_state()->globals()->host_resolver.reset(
        new net::MockHostResolver());

    BrowserWithTestWindowTest::SetUp();
  }

  virtual void TearDown() OVERRIDE {
    DestroyBrowserAndProfile();
    BrowserWithTestWindowTest::TearDown();

    testing_io_thread_state_.reset();

    profile_manager_.reset();
  }

 private:
  TestingBrowserProcess* browser_process() {
    return TestingBrowserProcess::GetGlobal();
  }

  scoped_ptr<TestingProfileManager> profile_manager_;
  scoped_ptr<chrome::TestingIOThreadState> testing_io_thread_state_;

  DISALLOW_COPY_AND_ASSIGN(OffTheRecordProfileImplTest);
};

// Test four things:
//  1. Host zoom maps of parent profile and child profile are different.
//  2. Child host zoom map inherites zoom level at construction.
//  3. Change of zoom level doesn't propagate from child to parent.
//  4. Change of zoom level propagate from parent to child.
TEST_F(OffTheRecordProfileImplTest, GetHostZoomMap) {
  // Constants for test case.
  std::string const host("example.com");
  double const zoom_level_25 = 2.5;
  double const zoom_level_30 = 3.0;
  double const zoom_level_40 = 4.0;

  // Prepare parent profile.
  scoped_ptr<Profile> parent_profile(new TestingProfileWithHostZoomMap);
  ASSERT_TRUE(parent_profile.get());
  ASSERT_TRUE(parent_profile->GetPrefs());
  ASSERT_TRUE(parent_profile->GetOffTheRecordPrefs());

  // Prepare parent host zoom map.
  HostZoomMap* parent_zoom_map =
      HostZoomMap::GetForBrowserContext(parent_profile.get());
  ASSERT_TRUE(parent_zoom_map);

  parent_zoom_map->SetZoomLevelForHost(host, zoom_level_25);
  ASSERT_EQ(parent_zoom_map->GetZoomLevelForHostAndScheme("http", host),
      zoom_level_25);

  // TODO(yosin) We need to wait ProfileImpl::Observe done for
  // OnZoomLevelChanged.

  // Prepare child profile as off the record profile.
  scoped_ptr<OffTheRecordProfileImpl> child_profile(
      new OffTheRecordProfileImpl(parent_profile.get()));
  child_profile->InitIoData();
  child_profile->InitHostZoomMap();

  BrowserContextDependencyManager::GetInstance()->
      CreateBrowserContextServicesForTest(child_profile.get());

  // Prepare child host zoom map.
  HostZoomMap* child_zoom_map =
      HostZoomMap::GetForBrowserContext(child_profile.get());
  ASSERT_TRUE(child_zoom_map);

  // Verity.
  EXPECT_NE(parent_zoom_map, child_zoom_map);

  EXPECT_EQ(parent_zoom_map->GetZoomLevelForHostAndScheme("http", host),
            child_zoom_map->GetZoomLevelForHostAndScheme("http", host)) <<
                "Child must inherit from parent.";

  child_zoom_map->SetZoomLevelForHost(host, zoom_level_30);
  ASSERT_EQ(
      child_zoom_map->GetZoomLevelForHostAndScheme("http", host),
      zoom_level_30);

  EXPECT_NE(parent_zoom_map->GetZoomLevelForHostAndScheme("http", host),
            child_zoom_map->GetZoomLevelForHostAndScheme("http", host)) <<
                "Child change must not propagate to parent.";

  parent_zoom_map->SetZoomLevelForHost(host, zoom_level_40);
  ASSERT_EQ(
      parent_zoom_map->GetZoomLevelForHostAndScheme("http", host),
      zoom_level_40);

  EXPECT_EQ(parent_zoom_map->GetZoomLevelForHostAndScheme("http", host),
            child_zoom_map->GetZoomLevelForHostAndScheme("http", host)) <<
                "Parent change should propagate to child.";
  base::RunLoop().RunUntilIdle();
}
