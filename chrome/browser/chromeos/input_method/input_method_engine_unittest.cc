// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/input_method_engine.h"
#include "chrome/browser/chromeos/input_method/input_method_engine_interface.h"
#include "chrome/browser/chromeos/input_method/mock_input_method_manager.h"
#include "chromeos/ime/extension_ime_util.h"
#include "chromeos/ime/mock_component_extension_ime_manager_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace input_method {
namespace {

const char kTestExtensionId[] = "mppnpdlheglhdfmldimlhpnegondlapf";
const char kTestExtensionId2[] = "dmpipdbjkoajgdeppkffbjhngfckdloi";
const char kTestImeEngineId[] = "test_engine_id";

enum CallsBitmap {
  NONE = 0U,
  ACTIVATE = 1U,
  DEACTIVATED = 2U,
  ONFOCUS = 4U,
  ONBLUR = 8U
};

void InitInputMethod() {
  ComponentExtensionIMEManager* comp_ime_manager =
      new ComponentExtensionIMEManager;
  MockComponentExtIMEManagerDelegate* delegate =
      new MockComponentExtIMEManagerDelegate;

  ComponentExtensionIME ext1;
  ext1.id = kTestExtensionId;

  ComponentExtensionEngine ext1_engine1;
  ext1_engine1.engine_id = kTestImeEngineId;
  ext1_engine1.language_codes.push_back("en-US");
  ext1_engine1.layouts.push_back("us");
  ext1.engines.push_back(ext1_engine1);

  std::vector<ComponentExtensionIME> ime_list;
  ime_list.push_back(ext1);
  delegate->set_ime_list(ime_list);
  comp_ime_manager->Initialize(
      scoped_ptr<ComponentExtensionIMEManagerDelegate>(delegate).Pass());

  MockInputMethodManager* manager = new MockInputMethodManager;
  manager->SetComponentExtensionIMEManager(
      scoped_ptr<ComponentExtensionIMEManager>(comp_ime_manager).Pass());
  InitializeForTesting(manager);
}

class TestObserver : public InputMethodEngineInterface::Observer {
 public:
  TestObserver() : calls_bitmap_(NONE) {}
  virtual ~TestObserver() {}

  virtual void OnActivate(const std::string& engine_id) OVERRIDE {
    calls_bitmap_ |= ACTIVATE;
  }
  virtual void OnDeactivated(const std::string& engine_id) OVERRIDE {
    calls_bitmap_ |= DEACTIVATED;
  }
  virtual void OnFocus(
      const InputMethodEngineInterface::InputContext& context) OVERRIDE {
    calls_bitmap_ |= ONFOCUS;
  }
  virtual void OnBlur(int context_id) OVERRIDE {
    calls_bitmap_ |= ONBLUR;
  }
  virtual void OnKeyEvent(
      const std::string& engine_id,
      const InputMethodEngineInterface::KeyboardEvent& event,
      input_method::KeyEventHandle* key_data) OVERRIDE {}
  virtual void OnInputContextUpdate(
      const InputMethodEngineInterface::InputContext& context) OVERRIDE {}
  virtual void OnCandidateClicked(
      const std::string& engine_id,
      int candidate_id,
      InputMethodEngineInterface::MouseButtonEvent button) OVERRIDE {}
  virtual void OnMenuItemActivated(
      const std::string& engine_id,
      const std::string& menu_id) OVERRIDE {}
  virtual void OnSurroundingTextChanged(
      const std::string& engine_id,
      const std::string& text,
      int cursor_pos,
      int anchor_pos) OVERRIDE {}
  virtual void OnReset(const std::string& engine_id) OVERRIDE {}

  unsigned char GetCallsBitmapAndReset() {
    unsigned char ret = calls_bitmap_;
    calls_bitmap_ = NONE;
    return ret;
  }

 private:
  unsigned char calls_bitmap_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

class InputMethodEngineTest :  public testing::Test {
 public:
  InputMethodEngineTest() : observer_(NULL), input_view_("inputview.html") {
    languages_.push_back("en-US");
    layouts_.push_back("us");
    InitInputMethod();
  }
  virtual ~InputMethodEngineTest() {
    engine_.reset();
    Shutdown();
  }

 protected:
  void CreateEngine(bool whitelisted) {
    engine_.reset(new InputMethodEngine());
    observer_ = new TestObserver();
    scoped_ptr<InputMethodEngineInterface::Observer> observer_ptr(observer_);
    engine_->Initialize(observer_ptr.Pass(),
                        "",
                        whitelisted ? kTestExtensionId : kTestExtensionId2,
                        kTestImeEngineId,
                        languages_,
                        layouts_,
                        options_page_,
                        input_view_);
  }

  void FocusIn(ui::TextInputType input_type) {
    IMEEngineHandlerInterface::InputContext input_context(
        input_type, ui::TEXT_INPUT_MODE_DEFAULT);
    engine_->FocusIn(input_context);
  }

  scoped_ptr<InputMethodEngine> engine_;
  TestObserver* observer_;
  std::vector<std::string> languages_;
  std::vector<std::string> layouts_;
  GURL options_page_;
  GURL input_view_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InputMethodEngineTest);
};

}  // namespace

TEST_F(InputMethodEngineTest, TestSwitching) {
  CreateEngine(false);
  // Enable/disable with focus.
  FocusIn(ui::TEXT_INPUT_TYPE_URL);
  EXPECT_EQ(NONE, observer_->GetCallsBitmapAndReset());
  engine_->Enable();
  EXPECT_EQ(ACTIVATE | ONFOCUS, observer_->GetCallsBitmapAndReset());
  engine_->Disable();
  EXPECT_EQ(DEACTIVATED, observer_->GetCallsBitmapAndReset());
  // Enable/disable without focus.
  engine_->FocusOut();
  EXPECT_EQ(NONE, observer_->GetCallsBitmapAndReset());
  engine_->Enable();
  EXPECT_EQ(ACTIVATE | ONFOCUS, observer_->GetCallsBitmapAndReset());
  engine_->Disable();
  EXPECT_EQ(DEACTIVATED, observer_->GetCallsBitmapAndReset());
  // Focus change when enabled.
  engine_->Enable();
  EXPECT_EQ(ACTIVATE | ONFOCUS, observer_->GetCallsBitmapAndReset());
  engine_->FocusOut();
  EXPECT_EQ(ONBLUR, observer_->GetCallsBitmapAndReset());
  // Focus change when disabled.
  engine_->Disable();
  EXPECT_EQ(DEACTIVATED, observer_->GetCallsBitmapAndReset());
  FocusIn(ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_EQ(NONE, observer_->GetCallsBitmapAndReset());
  engine_->FocusOut();
  EXPECT_EQ(NONE, observer_->GetCallsBitmapAndReset());
}

TEST_F(InputMethodEngineTest, TestSwitching_Password_3rd_Party) {
  CreateEngine(false);
  // Enable/disable with focus.
  FocusIn(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(NONE, observer_->GetCallsBitmapAndReset());
  engine_->Enable();
  EXPECT_EQ(ACTIVATE, observer_->GetCallsBitmapAndReset());
  engine_->Disable();
  EXPECT_EQ(DEACTIVATED, observer_->GetCallsBitmapAndReset());
  // Focus change when enabled.
  engine_->Enable();
  EXPECT_EQ(ACTIVATE, observer_->GetCallsBitmapAndReset());
  engine_->FocusOut();
  EXPECT_EQ(NONE, observer_->GetCallsBitmapAndReset());
  FocusIn(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(NONE, observer_->GetCallsBitmapAndReset());
  engine_->Disable();
  EXPECT_EQ(DEACTIVATED, observer_->GetCallsBitmapAndReset());
}

TEST_F(InputMethodEngineTest, TestSwitching_Password_Whitelisted) {
  CreateEngine(true);
  // Enable/disable with focus.
  FocusIn(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(NONE, observer_->GetCallsBitmapAndReset());
  engine_->Enable();
  EXPECT_EQ(ACTIVATE | ONFOCUS, observer_->GetCallsBitmapAndReset());
  engine_->Disable();
  EXPECT_EQ(DEACTIVATED, observer_->GetCallsBitmapAndReset());
  // Focus change when enabled.
  engine_->Enable();
  EXPECT_EQ(ACTIVATE | ONFOCUS, observer_->GetCallsBitmapAndReset());
  engine_->FocusOut();
  EXPECT_EQ(ONBLUR, observer_->GetCallsBitmapAndReset());
  FocusIn(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(ONFOCUS, observer_->GetCallsBitmapAndReset());
  engine_->Disable();
  EXPECT_EQ(DEACTIVATED, observer_->GetCallsBitmapAndReset());
}

}  // namespace input_method
}  // namespace chromeos
