// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/input_method_engine.h"
#include "chrome/browser/chromeos/input_method/input_method_engine_interface.h"
#include "chrome/browser/chromeos/input_method/mock_input_method_manager.h"
#include "chromeos/ime/extension_ime_util.h"
#include "chromeos/ime/mock_component_extension_ime_manager_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/chromeos/mock_ime_input_context_handler.h"

namespace chromeos {

namespace input_method {
namespace {

const char kTestExtensionId[] = "mppnpdlheglhdfmldimlhpnegondlapf";
const char kTestExtensionId2[] = "dmpipdbjkoajgdeppkffbjhngfckdloi";
const char kTestImeComponentId[] = "test_engine_id";

const char* kHistogramNames[] = {
    "InputMethod.Enable.test_engine_id", "InputMethod.Commit.test_engine_id",
    "InputMethod.CommitCharacter.test_engine_id",
};

scoped_ptr<base::HistogramSamples> GetHistogramSamples(
    const char* histogram_name) {
  base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(histogram_name);
  EXPECT_NE(static_cast<base::HistogramBase*>(NULL), histogram);
  return histogram->SnapshotSamples().Pass();
}

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
  ext1_engine1.engine_id = kTestImeComponentId;
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
    IMEBridge::Initialize();
    mock_ime_input_context_handler_.reset(new MockIMEInputContextHandler());
    IMEBridge::Get()->SetInputContextHandler(
        mock_ime_input_context_handler_.get());

    base::StatisticsRecorder::Initialize();

    for (size_t i = 0; i < arraysize(kHistogramNames); i++) {
      base::Histogram::FactoryGet(
          kHistogramNames[i], 0, 1000000, 50, base::HistogramBase::kNoFlags)
          ->Add(0);
      initial_histogram_samples_[i] =
          GetHistogramSamples(kHistogramNames[i]).Pass();
      initial_histogram_samples_map_[kHistogramNames[i]] =
          initial_histogram_samples_[i].get();
    }
  }
  virtual ~InputMethodEngineTest() {
    IMEBridge::Get()->SetInputContextHandler(NULL);
    engine_.reset();
    Shutdown();
  }

 protected:
  scoped_ptr<base::HistogramSamples> GetHistogramSamplesDelta(
      const char* histogram_name) {
    scoped_ptr<base::HistogramSamples> delta_samples(
        GetHistogramSamples(histogram_name));
    delta_samples->Subtract(*initial_histogram_samples_map_[histogram_name]);

    return delta_samples.Pass();
  }

  void ExpectNewSample(const char* histogram_name,
                       base::HistogramBase::Sample sample,
                       int total_count,
                       int sample_count) {
    scoped_ptr<base::HistogramSamples> delta_samples(
        GetHistogramSamplesDelta(histogram_name));
    EXPECT_EQ(total_count, delta_samples->TotalCount());
    EXPECT_EQ(sample_count, delta_samples->GetCount(sample));
  }

  void CreateEngine(bool whitelisted) {
    engine_.reset(new InputMethodEngine());
    observer_ = new TestObserver();
    scoped_ptr<InputMethodEngineInterface::Observer> observer_ptr(observer_);
    engine_->Initialize(observer_ptr.Pass(),
                        whitelisted ? kTestExtensionId : kTestExtensionId2);
  }

  void FocusIn(ui::TextInputType input_type) {
    IMEEngineHandlerInterface::InputContext input_context(
        input_type, ui::TEXT_INPUT_MODE_DEFAULT);
    engine_->FocusIn(input_context);
    IMEBridge::Get()->SetCurrentTextInputType(input_type);
  }

  scoped_ptr<InputMethodEngine> engine_;

  TestObserver* observer_;
  std::vector<std::string> languages_;
  std::vector<std::string> layouts_;
  GURL options_page_;
  GURL input_view_;

  scoped_ptr<base::HistogramSamples>
      initial_histogram_samples_[arraysize(kHistogramNames)];
  std::map<std::string, base::HistogramSamples*> initial_histogram_samples_map_;

  scoped_ptr<MockIMEInputContextHandler> mock_ime_input_context_handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InputMethodEngineTest);
};

}  // namespace

TEST_F(InputMethodEngineTest, TestSwitching) {
  CreateEngine(false);
  // Enable/disable with focus.
  FocusIn(ui::TEXT_INPUT_TYPE_URL);
  EXPECT_EQ(NONE, observer_->GetCallsBitmapAndReset());
  engine_->Enable(kTestImeComponentId);
  EXPECT_EQ(ACTIVATE | ONFOCUS, observer_->GetCallsBitmapAndReset());
  engine_->Disable();
  EXPECT_EQ(DEACTIVATED, observer_->GetCallsBitmapAndReset());
  // Enable/disable without focus.
  engine_->FocusOut();
  EXPECT_EQ(NONE, observer_->GetCallsBitmapAndReset());
  engine_->Enable(kTestImeComponentId);
  EXPECT_EQ(ACTIVATE | ONFOCUS, observer_->GetCallsBitmapAndReset());
  engine_->Disable();
  EXPECT_EQ(DEACTIVATED, observer_->GetCallsBitmapAndReset());
  // Focus change when enabled.
  engine_->Enable(kTestImeComponentId);
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
  engine_->Enable(kTestImeComponentId);
  EXPECT_EQ(ACTIVATE | ONFOCUS, observer_->GetCallsBitmapAndReset());
  engine_->Disable();
  EXPECT_EQ(DEACTIVATED, observer_->GetCallsBitmapAndReset());
  // Focus change when enabled.
  engine_->Enable(kTestImeComponentId);
  EXPECT_EQ(ACTIVATE | ONFOCUS, observer_->GetCallsBitmapAndReset());
  engine_->FocusOut();
  EXPECT_EQ(ONBLUR, observer_->GetCallsBitmapAndReset());
  FocusIn(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(ONFOCUS, observer_->GetCallsBitmapAndReset());
  engine_->Disable();
  EXPECT_EQ(DEACTIVATED, observer_->GetCallsBitmapAndReset());
}

TEST_F(InputMethodEngineTest, TestSwitching_Password_Whitelisted) {
  CreateEngine(true);
  // Enable/disable with focus.
  FocusIn(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(NONE, observer_->GetCallsBitmapAndReset());
  engine_->Enable(kTestImeComponentId);
  EXPECT_EQ(ACTIVATE | ONFOCUS, observer_->GetCallsBitmapAndReset());
  engine_->Disable();
  EXPECT_EQ(DEACTIVATED, observer_->GetCallsBitmapAndReset());
  // Focus change when enabled.
  engine_->Enable(kTestImeComponentId);
  EXPECT_EQ(ACTIVATE | ONFOCUS, observer_->GetCallsBitmapAndReset());
  engine_->FocusOut();
  EXPECT_EQ(ONBLUR, observer_->GetCallsBitmapAndReset());
  FocusIn(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(ONFOCUS, observer_->GetCallsBitmapAndReset());
  engine_->Disable();
  EXPECT_EQ(DEACTIVATED, observer_->GetCallsBitmapAndReset());
}

TEST_F(InputMethodEngineTest, TestHistograms) {
  CreateEngine(true);
  FocusIn(ui::TEXT_INPUT_TYPE_TEXT);
  engine_->Enable(kTestImeComponentId);
  std::string error;
  ExpectNewSample("InputMethod.Enable.test_engine_id", 1, 1, 1);
  engine_->CommitText(1, "input", &error);
  engine_->CommitText(1, "入力", &error);
  engine_->CommitText(1, "input入力", &error);
  ExpectNewSample("InputMethod.Commit.test_engine_id", 1, 3, 3);
  ExpectNewSample("InputMethod.CommitCharacter.test_engine_id", 5, 3, 1);
  ExpectNewSample("InputMethod.CommitCharacter.test_engine_id", 2, 3, 1);
  ExpectNewSample("InputMethod.CommitCharacter.test_engine_id", 7, 3, 1);
}

}  // namespace input_method
}  // namespace chromeos
