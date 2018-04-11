// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"

#include <stddef.h>

#include "base/macros.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/vm_applications/apps.pb.h"
#include "components/crx_file/id_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using vm_tools::apps::App;
using vm_tools::apps::ApplicationList;

namespace chromeos {

class CrostiniRegistryServiceTest : public testing::Test {
 public:
  CrostiniRegistryServiceTest() = default;

  // testing::Test:
  void SetUp() override {
    service_ = std::make_unique<CrostiniRegistryService>(&profile_);
  }

 protected:
  class Observer : public CrostiniRegistryService::Observer {
   public:
    MOCK_METHOD4(OnRegistryUpdated,
                 void(CrostiniRegistryService*,
                      const std::vector<std::string>&,
                      const std::vector<std::string>&,
                      const std::vector<std::string>&));
  };

  std::string GenerateAppId(const std::string& desktop_file_id,
                            const std::string& vm_name,
                            const std::string& container_name) {
    return crx_file::id_util::GenerateId(
        "crostini:" + vm_name + "/" + container_name + "/" + desktop_file_id);
  }

  App BasicApp(const std::string& desktop_file_id) {
    App app;
    app.set_desktop_file_id(desktop_file_id);
    App::LocaleString::Entry* entry = app.mutable_name()->add_values();
    entry->set_locale(std::string());
    entry->set_value(desktop_file_id);
    return app;
  }

  // Returns an ApplicationList with a single desktop file
  ApplicationList BasicAppList(const std::string& desktop_file_id,
                               const std::string& vm_name,
                               const std::string& container_name) {
    ApplicationList app_list;
    app_list.set_vm_name(vm_name);
    app_list.set_container_name(container_name);
    *app_list.add_apps() = BasicApp(desktop_file_id);
    return app_list;
  }

  CrostiniRegistryService* service() { return service_.get(); }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;

  std::unique_ptr<CrostiniRegistryService> service_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniRegistryServiceTest);
};

TEST_F(CrostiniRegistryServiceTest, SetAndGetRegistration) {
  std::string desktop_file_id = "vim";
  std::string vm_name = "awesomevm";
  std::string container_name = "awesomecontainer";
  std::map<std::string, std::string> name = {{"", "Vim"}};
  std::map<std::string, std::string> comment = {
      {"", "Edit text files"},
      {"en_GB", "Modify files containing textual content"},
  };
  std::vector<std::string> mime_types = {"text/plain", "text/x-python"};
  bool no_display = true;

  std::string app_id = GenerateAppId(desktop_file_id, vm_name, container_name);
  EXPECT_EQ(nullptr, service()->GetRegistration(app_id));

  ApplicationList app_list;
  app_list.set_vm_name(vm_name);
  app_list.set_container_name(container_name);

  App* app = app_list.add_apps();
  app->set_desktop_file_id(desktop_file_id);
  app->set_no_display(no_display);

  for (const auto& localized_name : name) {
    App::LocaleString::Entry* entry = app->mutable_name()->add_values();
    entry->set_locale(localized_name.first);
    entry->set_value(localized_name.second);
  }

  for (const auto& localized_comment : comment) {
    App::LocaleString::Entry* entry = app->mutable_comment()->add_values();
    entry->set_locale(localized_comment.first);
    entry->set_value(localized_comment.second);
  }

  for (const std::string& mime_type : mime_types)
    app->add_mime_types(mime_type);

  service()->UpdateApplicationList(app_list);
  auto result = service()->GetRegistration(app_id);
  ASSERT_NE(nullptr, result);
  EXPECT_EQ(result->desktop_file_id, desktop_file_id);
  EXPECT_EQ(result->vm_name, vm_name);
  EXPECT_EQ(result->container_name, container_name);
  EXPECT_EQ(result->name, name);
  EXPECT_EQ(result->comment, comment);
  EXPECT_EQ(result->mime_types, mime_types);
  EXPECT_EQ(result->no_display, no_display);
}

TEST_F(CrostiniRegistryServiceTest, Observer) {
  ApplicationList app_list = BasicAppList("app 1", "vm", "container");
  *app_list.add_apps() = BasicApp("app 2");
  *app_list.add_apps() = BasicApp("app 3");
  std::string app_id_1 = GenerateAppId("app 1", "vm", "container");
  std::string app_id_2 = GenerateAppId("app 2", "vm", "container");
  std::string app_id_3 = GenerateAppId("app 3", "vm", "container");
  std::string app_id_4 = GenerateAppId("app 4", "vm", "container");

  Observer observer;
  service()->AddObserver(&observer);
  EXPECT_CALL(observer,
              OnRegistryUpdated(
                  service(), testing::IsEmpty(), testing::IsEmpty(),
                  testing::UnorderedElementsAre(app_id_1, app_id_2, app_id_3)));
  service()->UpdateApplicationList(app_list);

  // Rename desktop file for "app 2" to "app 4" (deletion+insertion)
  app_list.mutable_apps(1)->set_desktop_file_id("app 4");
  // Rename name for "app 3" to "banana"
  app_list.mutable_apps(2)->mutable_name()->mutable_values(0)->set_value(
      "banana");
  EXPECT_CALL(observer,
              OnRegistryUpdated(service(), testing::ElementsAre(app_id_3),
                                testing::ElementsAre(app_id_2),
                                testing::ElementsAre(app_id_4)));
  service()->UpdateApplicationList(app_list);
}

// Test that UpdateApplicationList doesn't clobber apps from different VMs or
// containers.
TEST_F(CrostiniRegistryServiceTest, MultipleContainers) {
  service()->UpdateApplicationList(BasicAppList("app", "vm 1", "container 1"));
  service()->UpdateApplicationList(BasicAppList("app", "vm 1", "container 2"));
  service()->UpdateApplicationList(BasicAppList("app", "vm 2", "container 1"));
  std::string app_id_1 = GenerateAppId("app", "vm 1", "container 1");
  std::string app_id_2 = GenerateAppId("app", "vm 1", "container 2");
  std::string app_id_3 = GenerateAppId("app", "vm 2", "container 1");

  EXPECT_THAT(service()->GetRegisteredAppIds(),
              testing::UnorderedElementsAre(app_id_1, app_id_2, app_id_3));

  // Clobber app_id_2
  service()->UpdateApplicationList(
      BasicAppList("app 2", "vm 1", "container 2"));
  std::string new_app_id = GenerateAppId("app 2", "vm 1", "container 2");

  EXPECT_THAT(service()->GetRegisteredAppIds(),
              testing::UnorderedElementsAre(app_id_1, app_id_3, new_app_id));
}

}  // namespace chromeos
