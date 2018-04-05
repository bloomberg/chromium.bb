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
  std::string GenerateAppId(const std::string& desktop_file_id,
                            const std::string& vm_name,
                            const std::string& container_name) {
    return crx_file::id_util::GenerateId(
        "crostini:" + vm_name + "/" + container_name + "/" + desktop_file_id);
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

}  // namespace chromeos
