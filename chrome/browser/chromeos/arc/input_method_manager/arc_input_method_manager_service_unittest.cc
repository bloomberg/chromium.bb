// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/input_method_manager/arc_input_method_manager_service.h"

#include <memory>
#include <tuple>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "components/arc/arc_features.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/test/test_browser_context.h"
#include "components/crx_file/id_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/mock_input_method_manager.h"

namespace arc {
namespace {

namespace im = chromeos::input_method;

// The fake im::InputMethodManager for testing.
class TestInputMethodManager : public im::MockInputMethodManager {
 public:
  // The fake im::InputMethodManager::State implementation for testing.
  class TestState : public im::MockInputMethodManager::State {
   public:
    TestState() : active_input_method_ids_() {}

    const std::vector<std::string>& GetActiveInputMethodIds() const override {
      return active_input_method_ids_;
    }

    im::InputMethodDescriptor GetCurrentInputMethod() const override {
      im::InputMethodDescriptor descriptor(
          active_ime_id_, "", "", std::vector<std::string>(),
          std::vector<std::string>(), false /* is_login_keyboard */, GURL(),
          GURL());
      return descriptor;
    }

    void AddActiveInputMethodId(const std::string& ime_id) {
      if (!std::count(active_input_method_ids_.begin(),
                      active_input_method_ids_.end(), ime_id)) {
        active_input_method_ids_.push_back(ime_id);
      }
    }

    void RemoveActiveInputMethodId(const std::string& ime_id) {
      active_input_method_ids_.erase(std::remove_if(
          active_input_method_ids_.begin(), active_input_method_ids_.end(),
          [&ime_id](const std::string& id) { return id == ime_id; }));
    }

    void SetActiveInputMethod(const std::string& ime_id) {
      active_ime_id_ = ime_id;
    }

   protected:
    friend base::RefCounted<InputMethodManager::State>;
    ~TestState() override = default;

   private:
    std::vector<std::string> active_input_method_ids_;
    std::string active_ime_id_ = "";
  };

  TestInputMethodManager() {
    state_ = scoped_refptr<TestState>(new TestState());
  }
  ~TestInputMethodManager() override = default;

  scoped_refptr<InputMethodManager::State> GetActiveIMEState() override {
    return state_;
  }

  TestState* state() { return state_.get(); }

 private:
  scoped_refptr<TestState> state_;

  DISALLOW_COPY_AND_ASSIGN(TestInputMethodManager);
};

class TestInputMethodManagerBridge : public ArcInputMethodManagerBridge {
 public:
  TestInputMethodManagerBridge() = default;
  ~TestInputMethodManagerBridge() override = default;

  void SendEnableIme(const std::string& ime_id,
                     bool enable,
                     EnableImeCallback callback) override {
    enable_ime_calls_.push_back(std::make_tuple(ime_id, enable));
    std::move(callback).Run(true);
  }
  void SendSwitchImeTo(const std::string& ime_id,
                       SwitchImeToCallback callback) override {
    switch_ime_to_calls_.push_back(ime_id);
    std::move(callback).Run(true);
  }

  std::vector<std::tuple<std::string, bool>> enable_ime_calls_;
  std::vector<std::string> switch_ime_to_calls_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestInputMethodManagerBridge);
};

class ArcInputMethodManagerServiceTest : public testing::Test {
 protected:
  ArcInputMethodManagerServiceTest()
      : arc_service_manager_(std::make_unique<ArcServiceManager>()) {}
  ~ArcInputMethodManagerServiceTest() override {}

  ArcInputMethodManagerService* service() { return service_; }

  TestInputMethodManagerBridge* bridge() { return test_bridge_; }

  TestInputMethodManager* imm() { return input_method_manager_; }

  void SetUp() override {
    input_method_manager_ = new TestInputMethodManager();
    chromeos::input_method::InputMethodManager::Initialize(
        input_method_manager_);

    context_ = std::make_unique<TestBrowserContext>();

    service_ = ArcInputMethodManagerService::GetForBrowserContextForTesting(
        context_.get());

    test_bridge_ = new TestInputMethodManagerBridge();
    service_->SetInputMethodManagerBridgeForTesting(
        base::WrapUnique(test_bridge_));
  }

  void TearDown() override {
    test_bridge_ = nullptr;
    service_->Shutdown();

    context_.reset(nullptr);

    chromeos::input_method::InputMethodManager::Shutdown();
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  std::unique_ptr<TestBrowserContext> context_;
  TestInputMethodManager* input_method_manager_ = nullptr;
  TestInputMethodManagerBridge* test_bridge_ = nullptr;  // Owned by |service_|

  ArcInputMethodManagerService* service_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ArcInputMethodManagerServiceTest);
};

}  // anonymous namespace

TEST_F(ArcInputMethodManagerServiceTest, ConstructAndDestruct) {
  // These two method are not implemented yet.
  ASSERT_TRUE(service() != nullptr);
  service()->OnActiveImeChanged("");
  service()->OnImeInfoChanged({});
  SUCCEED();
}

TEST_F(ArcInputMethodManagerServiceTest, EnableIme) {
  using namespace chromeos::extension_ime_util;
  using crx_file::id_util::GenerateId;

  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeature(kEnableInputMethodFeature);

  ASSERT_EQ(0u, bridge()->enable_ime_calls_.size());

  const std::string extension_ime_id =
      GetInputMethodID(GenerateId("test.extension.ime"), "us");
  const std::string component_extension_ime_id = GetComponentInputMethodID(
      GenerateId("test.component.extension.ime"), "us");
  const std::string arc_ime_id =
      GetArcInputMethodID(GenerateId("test.arc.ime"), "us");

  // EnableIme is called only when ARC IME is enable or disabled.
  imm()->state()->AddActiveInputMethodId(extension_ime_id);
  service()->ImeMenuListChanged();
  EXPECT_EQ(0u, bridge()->enable_ime_calls_.size());

  imm()->state()->AddActiveInputMethodId(component_extension_ime_id);
  service()->ImeMenuListChanged();
  EXPECT_EQ(0u, bridge()->enable_ime_calls_.size());

  // Enable the ARC IME and verify that EnableIme is called.
  imm()->state()->AddActiveInputMethodId(arc_ime_id);
  service()->ImeMenuListChanged();
  ASSERT_EQ(1u, bridge()->enable_ime_calls_.size());
  EXPECT_EQ(GetComponentIDByInputMethodID(arc_ime_id),
            std::get<std::string>(bridge()->enable_ime_calls_[0]));
  EXPECT_TRUE(std::get<bool>(bridge()->enable_ime_calls_[0]));

  // Disable the ARC IME and verify that EnableIme is called with false.
  imm()->state()->RemoveActiveInputMethodId(arc_ime_id);
  service()->ImeMenuListChanged();
  ASSERT_EQ(2u, bridge()->enable_ime_calls_.size());
  EXPECT_EQ(GetComponentIDByInputMethodID(arc_ime_id),
            std::get<std::string>(bridge()->enable_ime_calls_[1]));
  EXPECT_FALSE(std::get<bool>(bridge()->enable_ime_calls_[1]));

  // EnableIme is not called when non ARC IME is disable.
  imm()->state()->RemoveActiveInputMethodId(extension_ime_id);
  service()->ImeMenuListChanged();
  EXPECT_EQ(2u, bridge()->enable_ime_calls_.size());
}

TEST_F(ArcInputMethodManagerServiceTest, SwitchImeTo) {
  using namespace chromeos::extension_ime_util;
  using crx_file::id_util::GenerateId;

  const std::string arc_ime_service_id =
      "org.chromium.arc.ime/.ArcInputMethodService";

  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeature(kEnableInputMethodFeature);

  ASSERT_EQ(0u, bridge()->switch_ime_to_calls_.size());

  const std::string extension_ime_id =
      GetInputMethodID(GenerateId("test.extension.ime"), "us");
  const std::string component_extension_ime_id = GetComponentInputMethodID(
      GenerateId("test.component.extension.ime"), "us");
  const std::string arc_ime_id = GetArcInputMethodID(GenerateId("test.arc.ime"),
                                                     "ime.id.in.arc.container");

  // Set active input method to the extension ime.
  imm()->state()->SetActiveInputMethod(extension_ime_id);
  service()->InputMethodChanged(imm(), nullptr, false /* show_message */);
  // ArcImeService should be selected.
  ASSERT_EQ(1u, bridge()->switch_ime_to_calls_.size());
  EXPECT_EQ(arc_ime_service_id, bridge()->switch_ime_to_calls_[0]);

  // Set active input method to the component extension ime.
  imm()->state()->SetActiveInputMethod(component_extension_ime_id);
  service()->InputMethodChanged(imm(), nullptr, false /* show_message */);
  // ArcImeService should be selected.
  ASSERT_EQ(2u, bridge()->switch_ime_to_calls_.size());
  EXPECT_EQ(arc_ime_service_id, bridge()->switch_ime_to_calls_[1]);

  // Set active input method to the arc ime.
  imm()->state()->SetActiveInputMethod(arc_ime_id);
  service()->InputMethodChanged(imm(), nullptr, false /* show_message */);
  ASSERT_EQ(3u, bridge()->switch_ime_to_calls_.size());
  EXPECT_EQ("ime.id.in.arc.container", bridge()->switch_ime_to_calls_[2]);
}

}  // namespace arc
