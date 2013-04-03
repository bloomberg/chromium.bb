// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chromeos/ime/component_extension_ime_manager.h"
#include "chromeos/ime/extension_ime_util.h"
#include "chromeos/ime/mock_component_extension_ime_manager_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace input_method {

namespace {

class TestableComponentExtensionIMEManager
    : public ComponentExtensionIMEManager {
 public:
  using ComponentExtensionIMEManager::GetComponentExtensionIMEId;
};

class ComponentExtensionIMEManagerTest :
    public testing::Test,
    public ComponentExtensionIMEManager::Observer {
 public:
  ComponentExtensionIMEManagerTest()
      : mock_delegate_(NULL),
        on_initialized_callcount_(0) {
  }

  void SetUp() {
    ime_list_.clear();

    ComponentExtensionIME ext1;
    ext1.id = "ext1_id";
    ext1.description = "ext1_description";
    ext1.path = base::FilePath("ext1_file_path");

    IBusComponent::EngineDescription ext1_engine1;
    ext1_engine1.engine_id = "ext1_engine1_engine_id";
    ext1_engine1.display_name = "ext1_engine_1_display_name";
    ext1_engine1.language_code = "en";
    ext1_engine1.layout = "us";
    ext1.engines.push_back(ext1_engine1);

    IBusComponent::EngineDescription ext1_engine2;
    ext1_engine2.engine_id = "ext1_engine2_engine_id";
    ext1_engine2.display_name = "ext1_engine2_display_name";
    ext1_engine2.language_code = "en";
    ext1_engine2.layout = "us";
    ext1.engines.push_back(ext1_engine2);

    IBusComponent::EngineDescription ext1_engine3;
    ext1_engine3.engine_id = "ext1_engine3_engine_id";
    ext1_engine3.display_name = "ext1_engine3_display_name";
    ext1_engine3.language_code = "ja";
    ext1_engine3.layout = "us";
    ext1.engines.push_back(ext1_engine3);

    ime_list_.push_back(ext1);

    ComponentExtensionIME ext2;
    ext2.id = "ext2_id";
    ext2.description = "ext2_description";
    ext2.path = base::FilePath("ext2_file_path");

    IBusComponent::EngineDescription ext2_engine1;
    ext2_engine1.engine_id = "ext2_engine1_engine_id";
    ext2_engine1.display_name = "ext2_engine_1_display_name";
    ext2_engine1.language_code = "en";
    ext2_engine1.layout = "us";
    ext2.engines.push_back(ext2_engine1);

    IBusComponent::EngineDescription ext2_engine2;
    ext2_engine2.engine_id = "ext2_engine2_engine_id";
    ext2_engine2.display_name = "ext2_engine2_display_name";
    ext2_engine2.language_code = "hi";
    ext2_engine2.layout = "us";
    ext2.engines.push_back(ext2_engine2);

    IBusComponent::EngineDescription ext2_engine3;
    ext2_engine3.engine_id = "ext2_engine3_engine_id";
    ext2_engine3.display_name = "ext2_engine3_display_name";
    ext2_engine3.language_code = "ja";
    ext2_engine3.layout = "jp";
    ext2.engines.push_back(ext2_engine3);

    ime_list_.push_back(ext2);

    ComponentExtensionIME ext3;
    ext3.id = "ext3_id";
    ext3.description = "ext3_description";
    ext3.path = base::FilePath("ext3_file_path");

    IBusComponent::EngineDescription ext3_engine1;
    ext3_engine1.engine_id = "ext3_engine1_engine_id";
    ext3_engine1.display_name = "ext3_engine_1_display_name";
    ext3_engine1.language_code = "hi";
    ext3_engine1.layout = "us";
    ext3.engines.push_back(ext3_engine1);

    IBusComponent::EngineDescription ext3_engine2;
    ext3_engine2.engine_id = "ext3_engine2_engine_id";
    ext3_engine2.display_name = "ext3_engine2_display_name";
    ext3_engine2.language_code = "en";
    ext3_engine2.layout = "us";
    ext3.engines.push_back(ext3_engine2);

    IBusComponent::EngineDescription ext3_engine3;
    ext3_engine3.engine_id = "ext3_engine3_engine_id";
    ext3_engine3.display_name = "ext3_engine3_display_name";
    ext3_engine3.language_code = "en";
    ext3_engine3.layout = "us";
    ext3.engines.push_back(ext3_engine3);

    ime_list_.push_back(ext3);

    mock_delegate_ = new MockComponentExtIMEManagerDelegate();
    mock_delegate_->set_ime_list(ime_list_);
    component_ext_mgr_.reset(new ComponentExtensionIMEManager());
    component_ext_mgr_->AddObserver(this);
    EXPECT_FALSE(component_ext_mgr_->IsInitialized());
    component_ext_mgr_->Initialize(
        scoped_ptr<ComponentExtensionIMEManagerDelegate>(
            mock_delegate_).Pass());
    EXPECT_TRUE(component_ext_mgr_->IsInitialized());

  }

  void TearDown() {
    EXPECT_EQ(1, on_initialized_callcount_);
    component_ext_mgr_->RemoveObserver(this);
  }

 protected:
  MockComponentExtIMEManagerDelegate* mock_delegate_;
  scoped_ptr<ComponentExtensionIMEManager> component_ext_mgr_;
  std::vector<ComponentExtensionIME> ime_list_;

 private:
  void OnInitialized() {
    ++on_initialized_callcount_;
  }

  int on_initialized_callcount_;

  DISALLOW_COPY_AND_ASSIGN(ComponentExtensionIMEManagerTest);
};

TEST_F(ComponentExtensionIMEManagerTest, LoadComponentExtensionIMETest) {
  for (size_t i = 0; i < ime_list_.size(); ++i) {
    for (size_t j = 0; j < ime_list_[i].engines.size(); ++j) {
      const std::string input_method_id =
          TestableComponentExtensionIMEManager::GetComponentExtensionIMEId(
              ime_list_[i].id,
              ime_list_[i].engines[j].engine_id);
      component_ext_mgr_->LoadComponentExtensionIME(input_method_id);
      EXPECT_EQ(ime_list_[i].id, mock_delegate_->last_loaded_extension_id());
    }
  }
  EXPECT_EQ(9, mock_delegate_->load_call_count());
}

TEST_F(ComponentExtensionIMEManagerTest, UnloadComponentExtensionIMETest) {
  for (size_t i = 0; i < ime_list_.size(); ++i) {
    for (size_t j = 0; j < ime_list_[i].engines.size(); ++j) {
      const std::string input_method_id =
          TestableComponentExtensionIMEManager::GetComponentExtensionIMEId(
              ime_list_[i].id,
              ime_list_[i].engines[j].engine_id);
      component_ext_mgr_->UnloadComponentExtensionIME(input_method_id);
      EXPECT_EQ(ime_list_[i].id, mock_delegate_->last_unloaded_extension_id());
    }
  }
  EXPECT_EQ(9, mock_delegate_->unload_call_count());
}

TEST_F(ComponentExtensionIMEManagerTest, IsWhitelistedTest) {
  EXPECT_TRUE(component_ext_mgr_->IsWhitelisted(
      TestableComponentExtensionIMEManager::GetComponentExtensionIMEId(
          ime_list_[0].id,
          ime_list_[0].engines[0].engine_id)));
  EXPECT_FALSE(component_ext_mgr_->IsWhitelisted(
      extension_ime_util::GetInputMethodID(
          ime_list_[0].id,
          ime_list_[0].engines[0].engine_id)));
  EXPECT_FALSE(component_ext_mgr_->IsWhitelisted("mozc"));
  EXPECT_FALSE(component_ext_mgr_->IsWhitelisted(
      extension_ime_util::GetInputMethodID("AAAA", "012345")));
  EXPECT_FALSE(component_ext_mgr_->IsWhitelisted(
      TestableComponentExtensionIMEManager::GetComponentExtensionIMEId(
          "AAAA", "012345")));
}

TEST_F(ComponentExtensionIMEManagerTest, IsWhitelistedExtensionTest) {
  EXPECT_TRUE(component_ext_mgr_->IsWhitelistedExtension(ime_list_[0].id));
  EXPECT_TRUE(component_ext_mgr_->IsWhitelistedExtension(ime_list_[1].id));
  EXPECT_FALSE(component_ext_mgr_->IsWhitelistedExtension("dummy"));
  EXPECT_FALSE(component_ext_mgr_->IsWhitelistedExtension(""));
}

TEST_F(ComponentExtensionIMEManagerTest, GetNameDescriptionTest) {
  for (size_t i = 0; i < ime_list_.size(); ++i) {
    for (size_t j = 0; j < ime_list_[i].engines.size(); ++j) {
      const IBusComponent::EngineDescription& engine
          = ime_list_[i].engines[j];

      const std::string input_method_id =
          TestableComponentExtensionIMEManager::GetComponentExtensionIMEId(
              ime_list_[i].id,
              engine.engine_id);

      EXPECT_EQ(input_method_id,
                component_ext_mgr_->GetId(ime_list_[i].id, engine.engine_id));
      EXPECT_EQ(engine.display_name,
                component_ext_mgr_->GetName(input_method_id));
      EXPECT_EQ(engine.description,
                component_ext_mgr_->GetDescription(input_method_id));
    }
  }
}

TEST_F(ComponentExtensionIMEManagerTest, ListIMEByLanguageTest) {
  const std::string hindi_layout1 =
      TestableComponentExtensionIMEManager::GetComponentExtensionIMEId(
          ime_list_[1].id, ime_list_[1].engines[1].engine_id);
  const std::string hindi_layout2 =
      TestableComponentExtensionIMEManager::GetComponentExtensionIMEId(
          ime_list_[2].id, ime_list_[2].engines[0].engine_id);

  std::vector<std::string> hindi_list
      = component_ext_mgr_->ListIMEByLanguage("hi");
  ASSERT_EQ(2UL, hindi_list.size());
  EXPECT_TRUE(hindi_list[0] == hindi_layout1 || hindi_list[0] == hindi_layout2);
  EXPECT_TRUE(hindi_list[1] == hindi_layout1 || hindi_list[1] == hindi_layout2);

  EXPECT_EQ(0UL, component_ext_mgr_->ListIMEByLanguage("ru").size());
  EXPECT_EQ(0UL, component_ext_mgr_->ListIMEByLanguage("").size());
  EXPECT_EQ(0UL, component_ext_mgr_->ListIMEByLanguage("invalid").size());
  EXPECT_EQ(5UL, component_ext_mgr_->ListIMEByLanguage("en").size());
  EXPECT_EQ(2UL, component_ext_mgr_->ListIMEByLanguage("ja").size());
}

TEST_F(ComponentExtensionIMEManagerTest, GetAllIMEAsInputMethodDescriptor) {
  input_method::InputMethodDescriptors descriptors =
      component_ext_mgr_->GetAllIMEAsInputMethodDescriptor();
  size_t total_ime_size = 0;
  for (size_t i = 0; i < ime_list_.size(); ++i) {
    total_ime_size += ime_list_[i].engines.size();
  }
  EXPECT_EQ(total_ime_size, descriptors.size());
}

TEST_F(ComponentExtensionIMEManagerTest, GetComponentExtensionIMEId) {
  const char kExtensionID[] = "extension_id";
  const char kEngineID[] = "engine_id";
  const std::string ime_id =
      TestableComponentExtensionIMEManager::GetComponentExtensionIMEId(
          kExtensionID, kEngineID);

  EXPECT_TRUE(ComponentExtensionIMEManager::IsComponentExtensionIMEId(
      ime_id));
  EXPECT_FALSE(ComponentExtensionIMEManager::IsComponentExtensionIMEId("mozc"));
  EXPECT_FALSE(ComponentExtensionIMEManager::IsComponentExtensionIMEId(
      extension_ime_util::GetInputMethodID(kExtensionID, kEngineID)));
  EXPECT_FALSE(ComponentExtensionIMEManager::IsComponentExtensionIMEId(
      extension_ime_util::GetInputMethodID(kExtensionID, "mozc")));
  EXPECT_FALSE(ComponentExtensionIMEManager::IsComponentExtensionIMEId(
      extension_ime_util::GetInputMethodID("ext-id", kEngineID)));
}

}  // namespace

}  // namespace input_method
}  // namespace chromeos
