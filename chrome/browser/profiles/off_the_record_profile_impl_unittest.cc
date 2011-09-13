// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/off_the_record_profile_impl.h"

#include "chrome/browser/net/ssl_config_service_manager.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/browser/host_zoom_map.h"

namespace {

class TestingProfileWithHostZoomMap : public TestingProfile,
                                      public NotificationObserver {
 public:
  TestingProfileWithHostZoomMap() {}

  virtual ~TestingProfileWithHostZoomMap() {}

  virtual HostZoomMap* GetHostZoomMap() {
    if (!host_zoom_map_) {
      host_zoom_map_ = new HostZoomMap();

      registrar_.Add(this, content::NOTIFICATION_ZOOM_LEVEL_CHANGED,
                     Source<HostZoomMap>(host_zoom_map_));
    }
    return host_zoom_map_.get();
  }

  virtual Profile* GetOffTheRecordProfile() OVERRIDE {
    if (!off_the_record_profile_.get())
      off_the_record_profile_.reset(CreateOffTheRecordProfile());
    return off_the_record_profile_.get();
  }

  virtual PrefService* GetOffTheRecordPrefs() OVERRIDE {
    return GetPrefs();
  }

  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE {
    const std::string& host = *(Details<const std::string>(details).ptr());
    if (type == content::NOTIFICATION_ZOOM_LEVEL_CHANGED) {
      if (!host.empty()) {
        double level = host_zoom_map_->GetZoomLevel(host);
        DictionaryPrefUpdate update(prefs_.get(), prefs::kPerHostZoomLevels);
        DictionaryValue* host_zoom_dictionary = update.Get();
        if (level == host_zoom_map_->default_zoom_level()) {
          host_zoom_dictionary->RemoveWithoutPathExpansion(host, NULL);
        } else {
          host_zoom_dictionary->SetWithoutPathExpansion(
              host, Value::CreateDoubleValue(level));
        }
      }
    }
  }

 private:
  NotificationRegistrar registrar_;
  scoped_refptr<HostZoomMap> host_zoom_map_;
  scoped_ptr<Profile> off_the_record_profile_;
  scoped_ptr<SSLConfigServiceManager> ssl_config_service_manager_;

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
    prefs_.reset(new TestingPrefService);
    browser::RegisterLocalState(prefs_.get());

    browser_process()->SetLocalState(prefs_.get());

    BrowserWithTestWindowTest::SetUp();
  }

  virtual void TearDown() OVERRIDE {
    BrowserWithTestWindowTest::TearDown();
    browser_process()->SetLocalState(NULL);
    DestroyBrowser();
    prefs_.reset();
  }

 private:
  TestingBrowserProcess* browser_process() {
    return static_cast<TestingBrowserProcess*>(g_browser_process);
  }

  scoped_ptr<PrefService> prefs_;

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
  HostZoomMap* parent_zoom_map = parent_profile->GetHostZoomMap();
  ASSERT_TRUE(parent_zoom_map);

  parent_zoom_map->SetZoomLevel(host, zoom_level_25);
  ASSERT_EQ(parent_zoom_map->GetZoomLevel(host), zoom_level_25);

  // TODO(yosin) We need to wait ProfileImpl::Observe done for
  // NOTIFICATION_ZOOM_LEVEL_CHANGED.

  // Prepare child profile as off the record profile.
  scoped_ptr<Profile> child_profile(
      new OffTheRecordProfileImpl(parent_profile.get()));

  // Prepare child host zoom map.
  HostZoomMap* child_zoom_map = child_profile->GetHostZoomMap();
  ASSERT_TRUE(child_zoom_map);

  // Verity.
  EXPECT_NE(parent_zoom_map, child_zoom_map);

  EXPECT_EQ(parent_zoom_map->GetZoomLevel(host),
            child_zoom_map->GetZoomLevel(host)) <<
                "Child must inherit from parent.";

  child_zoom_map->SetZoomLevel(host, zoom_level_30);
  ASSERT_EQ(child_zoom_map->GetZoomLevel(host), zoom_level_30);

  EXPECT_NE(parent_zoom_map->GetZoomLevel(host),
            child_zoom_map->GetZoomLevel(host)) <<
                "Child change must not propaget to parent.";

  parent_zoom_map->SetZoomLevel(host, zoom_level_40);
  ASSERT_EQ(parent_zoom_map->GetZoomLevel(host), zoom_level_40);

  EXPECT_EQ(parent_zoom_map->GetZoomLevel(host),
            child_zoom_map->GetZoomLevel(host)) <<
                "Parent change should propaget to child.";
}
