// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_manager_impl.h"

#include <algorithm>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/chromeos/input_method/input_method_engine_interface.h"
#include "chrome/browser/chromeos/input_method/mock_candidate_window_controller.h"
#include "chrome/browser/chromeos/input_method/mock_input_method_engine.h"
#include "chromeos/ime/extension_ime_util.h"
#include "chromeos/ime/fake_input_method_delegate.h"
#include "chromeos/ime/fake_xkeyboard.h"
#include "chromeos/ime/mock_component_extension_ime_manager_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/ime/chromeos/mock_ime_engine_handler.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace chromeos {

namespace input_method {
namespace {

const char kNaclMozcUsId[] =
    "_comp_ime_fpfbhcjppmaeaijcidgiibchfbnhbeljnacl_mozc_us";
const char kNaclMozcJpId[] =
    "_comp_ime_fpfbhcjppmaeaijcidgiibchfbnhbeljnacl_mozc_jp";
const char kExt2Engine1Id[] =
    "_comp_ime_nmblnjkfdkabgdofidlkienfnnbjhnabext2_engine1_engine_id";
const char kExt2Engine2Id[] =
    "_comp_ime_nmblnjkfdkabgdofidlkienfnnbjhnabext2_engine2_engine_id";

// Returns true if |descriptors| contain |target|.
bool Contain(const InputMethodDescriptors& descriptors,
             const InputMethodDescriptor& target) {
  for (size_t i = 0; i < descriptors.size(); ++i) {
    if (descriptors[i].id() == target.id())
      return true;
  }
  return false;
}

class InputMethodManagerImplTest :  public testing::Test {
 public:
  InputMethodManagerImplTest()
      : delegate_(NULL),
        candidate_window_controller_(NULL),
        xkeyboard_(NULL) {
  }
  virtual ~InputMethodManagerImplTest() {}

  virtual void SetUp() OVERRIDE {
    delegate_ = new FakeInputMethodDelegate();
    manager_.reset(new InputMethodManagerImpl(
        scoped_ptr<InputMethodDelegate>(delegate_)));
    manager_->GetInputMethodUtil()->UpdateHardwareLayoutCache();
    candidate_window_controller_ = new MockCandidateWindowController;
    manager_->SetCandidateWindowControllerForTesting(
        candidate_window_controller_);
    xkeyboard_ = new FakeXKeyboard;
    manager_->SetXKeyboardForTesting(xkeyboard_);
    mock_engine_handler_.reset(new MockIMEEngineHandler());
    IMEBridge::Initialize();
    IMEBridge::Get()->SetCurrentEngineHandler(mock_engine_handler_.get());

    ime_list_.clear();

    ComponentExtensionIME ext1;
    ext1.id = "fpfbhcjppmaeaijcidgiibchfbnhbelj";
    ext1.description = "ext1_description";
    ext1.path = base::FilePath("ext1_file_path");

    ComponentExtensionEngine ext1_engine1;
    ext1_engine1.engine_id = "nacl_mozc_us";
    ext1_engine1.display_name = "ext1_engine_1_display_name";
    ext1_engine1.language_codes.push_back("ja");
    ext1_engine1.layouts.push_back("us");
    ext1.engines.push_back(ext1_engine1);

    ComponentExtensionEngine ext1_engine2;
    ext1_engine2.engine_id = "nacl_mozc_jp";
    ext1_engine2.display_name = "ext1_engine_1_display_name";
    ext1_engine2.language_codes.push_back("ja");
    ext1_engine2.layouts.push_back("jp");
    ext1.engines.push_back(ext1_engine2);

    ime_list_.push_back(ext1);

    ComponentExtensionIME ext2;
    ext2.id = "nmblnjkfdkabgdofidlkienfnnbjhnab";
    ext2.description = "ext2_description";
    ext2.path = base::FilePath("ext2_file_path");

    ComponentExtensionEngine ext2_engine1;
    ext2_engine1.engine_id = "ext2_engine1_engine_id";
    ext2_engine1.display_name = "ext2_engine_1_display_name";
    ext2_engine1.language_codes.push_back("en");
    ext2_engine1.layouts.push_back("us");
    ext2.engines.push_back(ext2_engine1);

    ComponentExtensionEngine ext2_engine2;
    ext2_engine2.engine_id = "ext2_engine2_engine_id";
    ext2_engine2.display_name = "ext2_engine_2_display_name";
    ext2_engine2.language_codes.push_back("en");
    ext2_engine2.layouts.push_back("us(dvorak)");
    ext2.engines.push_back(ext2_engine2);

    ime_list_.push_back(ext2);
  }

  virtual void TearDown() OVERRIDE {
    delegate_ = NULL;
    candidate_window_controller_ = NULL;
    xkeyboard_ = NULL;
    manager_.reset();
    IMEBridge::Get()->SetCurrentEngineHandler(NULL);
    IMEBridge::Shutdown();
  }

 protected:
  // Helper function to initialize component extension stuff for testing.
  void InitComponentExtension() {
    mock_delegate_ = new MockComponentExtIMEManagerDelegate();
    mock_delegate_->set_ime_list(ime_list_);
    scoped_ptr<ComponentExtensionIMEManagerDelegate> delegate(mock_delegate_);
    // Note, for production, these SetEngineHandler are called when
    // IMEEngineHandlerInterface is initialized via
    // InitializeComponentextension.
    IMEBridge::Get()->SetEngineHandler(kNaclMozcUsId,
                                        mock_engine_handler_.get());
    IMEBridge::Get()->SetEngineHandler(kNaclMozcJpId,
                                        mock_engine_handler_.get());
    IMEBridge::Get()->SetEngineHandler(kExt2Engine1Id,
                                        mock_engine_handler_.get());
    IMEBridge::Get()->SetEngineHandler(kExt2Engine2Id,
                                        mock_engine_handler_.get());
    manager_->InitializeComponentExtensionForTesting(delegate.Pass());
  }

  scoped_ptr<InputMethodManagerImpl> manager_;
  FakeInputMethodDelegate* delegate_;
  MockCandidateWindowController* candidate_window_controller_;
  scoped_ptr<MockIMEEngineHandler> mock_engine_handler_;
  FakeXKeyboard* xkeyboard_;
  base::MessageLoop message_loop_;
  MockComponentExtIMEManagerDelegate* mock_delegate_;
  std::vector<ComponentExtensionIME> ime_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InputMethodManagerImplTest);
};

class TestObserver : public InputMethodManager::Observer {
 public:
  TestObserver()
      : input_method_changed_count_(0),
        input_method_property_changed_count_(0),
        last_show_message_(false) {
  }
  virtual ~TestObserver() {}

  virtual void InputMethodChanged(InputMethodManager* manager,
                                  bool show_message) OVERRIDE {
    ++input_method_changed_count_;
    last_show_message_ = show_message;
  }
  virtual void InputMethodPropertyChanged(
      InputMethodManager* manager) OVERRIDE {
    ++input_method_property_changed_count_;
  }

  int input_method_changed_count_;
  int input_method_property_changed_count_;
  bool last_show_message_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

class TestCandidateWindowObserver
    : public InputMethodManager::CandidateWindowObserver {
 public:
  TestCandidateWindowObserver()
      : candidate_window_opened_count_(0),
        candidate_window_closed_count_(0) {
  }
  virtual ~TestCandidateWindowObserver() {}

  virtual void CandidateWindowOpened(InputMethodManager* manager) OVERRIDE {
    ++candidate_window_opened_count_;
  }
  virtual void CandidateWindowClosed(InputMethodManager* manager) OVERRIDE {
    ++candidate_window_closed_count_;
  }

  int candidate_window_opened_count_;
  int candidate_window_closed_count_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestCandidateWindowObserver);
};
}  // namespace

TEST_F(InputMethodManagerImplTest, TestGetXKeyboard) {
  EXPECT_TRUE(manager_->GetXKeyboard());
  EXPECT_EQ(xkeyboard_, manager_->GetXKeyboard());
}

TEST_F(InputMethodManagerImplTest, TestCandidateWindowObserver) {
  TestCandidateWindowObserver observer;
  candidate_window_controller_->NotifyCandidateWindowOpened();  // nop
  candidate_window_controller_->NotifyCandidateWindowClosed();  // nop
  manager_->AddCandidateWindowObserver(&observer);
  candidate_window_controller_->NotifyCandidateWindowOpened();
  EXPECT_EQ(1, observer.candidate_window_opened_count_);
  candidate_window_controller_->NotifyCandidateWindowClosed();
  EXPECT_EQ(1, observer.candidate_window_closed_count_);
  candidate_window_controller_->NotifyCandidateWindowOpened();
  EXPECT_EQ(2, observer.candidate_window_opened_count_);
  candidate_window_controller_->NotifyCandidateWindowClosed();
  EXPECT_EQ(2, observer.candidate_window_closed_count_);
  manager_->RemoveCandidateWindowObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestObserver) {
  // For http://crbug.com/19655#c11 - (3). browser_state_monitor_unittest.cc is
  // also for the scenario.
  std::vector<std::string> keyboard_layouts;
  keyboard_layouts.push_back("xkb:us::eng");

  TestObserver observer;
  InitComponentExtension();
  manager_->AddObserver(&observer);
  EXPECT_EQ(0, observer.input_method_changed_count_);
  manager_->EnableLoginLayouts("en-US", keyboard_layouts);
  EXPECT_EQ(1, observer.input_method_changed_count_);
  EXPECT_EQ(1, observer.input_method_property_changed_count_);
  manager_->ChangeInputMethod("xkb:us:dvorak:eng");
  EXPECT_FALSE(observer.last_show_message_);
  EXPECT_EQ(2, observer.input_method_changed_count_);
  EXPECT_EQ(2, observer.input_method_property_changed_count_);
  manager_->ChangeInputMethod("xkb:us:dvorak:eng");
  EXPECT_FALSE(observer.last_show_message_);

  // The observer is always notified even when the same input method ID is
  // passed to ChangeInputMethod() more than twice.
  // TODO(komatsu): Revisit if this is neccessary.
  EXPECT_EQ(3, observer.input_method_changed_count_);

  // If the same input method ID is passed, PropertyChanged() is not
  // notified.
  EXPECT_EQ(2, observer.input_method_property_changed_count_);

  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestGetSupportedInputMethods) {
  InitComponentExtension();
  scoped_ptr<InputMethodDescriptors> methods(
      manager_->GetSupportedInputMethods());
  ASSERT_TRUE(methods.get());
  // Try to find random 4-5 layuts and IMEs to make sure the returned list is
  // correct.
  const InputMethodDescriptor* id_to_find =
      manager_->GetInputMethodUtil()->GetInputMethodDescriptorFromId(
          kNaclMozcUsId);
  id_to_find = manager_->GetInputMethodUtil()->GetInputMethodDescriptorFromId(
      "xkb:us::eng");
  EXPECT_TRUE(Contain(*methods.get(), *id_to_find));
  id_to_find = manager_->GetInputMethodUtil()->GetInputMethodDescriptorFromId(
      "xkb:us:dvorak:eng");
  EXPECT_TRUE(Contain(*methods.get(), *id_to_find));
  id_to_find = manager_->GetInputMethodUtil()->GetInputMethodDescriptorFromId(
      "xkb:fr::fra");
  EXPECT_TRUE(Contain(*methods.get(), *id_to_find));
}

TEST_F(InputMethodManagerImplTest, TestEnableLayouts) {
  // Currently 5 keyboard layouts are supported for en-US, and 1 for ja. See
  // ibus_input_method.txt.
  std::vector<std::string> keyboard_layouts;

  InitComponentExtension();
  manager_->EnableLoginLayouts("en-US", keyboard_layouts);
  EXPECT_EQ(5U, manager_->GetNumActiveInputMethods());
  for (size_t i = 0; i < manager_->GetActiveInputMethodIds().size(); ++i)
    LOG(ERROR) << manager_->GetActiveInputMethodIds().at(i);

  // For http://crbug.com/19655#c11 - (5)
  // The hardware keyboard layout "xkb:us::eng" is always active, hence 2U.
  manager_->EnableLoginLayouts("ja", keyboard_layouts);  // Japanese
  EXPECT_EQ(2U, manager_->GetNumActiveInputMethods());
}

TEST_F(InputMethodManagerImplTest, TestEnableLayoutsAndCurrentInputMethod) {
  // For http://crbug.com/329061
  std::vector<std::string> keyboard_layouts;
  keyboard_layouts.push_back("xkb:se::swe");

  manager_->EnableLoginLayouts("en-US", keyboard_layouts);
  const std::string im_id = manager_->GetCurrentInputMethod().id();
  EXPECT_EQ("xkb:se::swe", im_id);
}

TEST_F(InputMethodManagerImplTest, TestEnableLayoutsNonUsHardwareKeyboard) {
  // The physical layout is French.
  manager_->GetInputMethodUtil()->SetHardwareKeyboardLayoutForTesting(
      "xkb:fr::fra");
  manager_->EnableLoginLayouts(
      "en-US",
      manager_->GetInputMethodUtil()->GetHardwareLoginInputMethodIds());
  EXPECT_EQ(6U, manager_->GetNumActiveInputMethods());  // 5 + French
  // The physical layout is Japanese.
  manager_->GetInputMethodUtil()->SetHardwareKeyboardLayoutForTesting(
      "xkb:jp::jpn");
  manager_->EnableLoginLayouts(
      "ja",
      manager_->GetInputMethodUtil()->GetHardwareLoginInputMethodIds());
  // "xkb:us::eng" is not needed, hence 1.
  EXPECT_EQ(1U, manager_->GetNumActiveInputMethods());

  // The physical layout is Russian.
  manager_->GetInputMethodUtil()->SetHardwareKeyboardLayoutForTesting(
      "xkb:ru::rus");
  manager_->EnableLoginLayouts(
      "ru",
      manager_->GetInputMethodUtil()->GetHardwareLoginInputMethodIds());
  // "xkb:us::eng" only.
  EXPECT_EQ(1U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ("xkb:us::eng", manager_->GetActiveInputMethodIds().front());
}

TEST_F(InputMethodManagerImplTest, TestEnableMultipleHardwareKeyboardLayout) {
  // The physical layouts are French and Hungarian.
  manager_->GetInputMethodUtil()->SetHardwareKeyboardLayoutForTesting(
      "xkb:fr::fra,xkb:hu::hun");
  manager_->EnableLoginLayouts(
      "en-US",
      manager_->GetInputMethodUtil()->GetHardwareLoginInputMethodIds());
  // 5 + French + Hungarian
  EXPECT_EQ(7U, manager_->GetNumActiveInputMethods());
}

TEST_F(InputMethodManagerImplTest,
       TestEnableMultipleHardwareKeyboardLayout_NoLoginKeyboard) {
  // The physical layouts are English (US) and Russian.
  manager_->GetInputMethodUtil()->SetHardwareKeyboardLayoutForTesting(
      "xkb:us::eng,xkb:ru::rus");
  manager_->EnableLoginLayouts(
      "ru",
      manager_->GetInputMethodUtil()->GetHardwareLoginInputMethodIds());
  // xkb:us:eng
  EXPECT_EQ(1U, manager_->GetNumActiveInputMethods());
}

TEST_F(InputMethodManagerImplTest, TestActiveInputMethods) {
  std::vector<std::string> keyboard_layouts;
  manager_->EnableLoginLayouts("ja", keyboard_layouts);  // Japanese
  EXPECT_EQ(2U, manager_->GetNumActiveInputMethods());
  scoped_ptr<InputMethodDescriptors> methods(
      manager_->GetActiveInputMethods());
  ASSERT_TRUE(methods.get());
  EXPECT_EQ(2U, methods->size());
  const InputMethodDescriptor* id_to_find =
      manager_->GetInputMethodUtil()->GetInputMethodDescriptorFromId(
          "xkb:us::eng");
  EXPECT_TRUE(Contain(*methods.get(), *id_to_find));
  id_to_find = manager_->GetInputMethodUtil()->GetInputMethodDescriptorFromId(
      "xkb:jp::jpn");
  EXPECT_TRUE(Contain(*methods.get(), *id_to_find));
}

TEST_F(InputMethodManagerImplTest, TestEnableTwoLayouts) {
  // For http://crbug.com/19655#c11 - (8), step 6.
  TestObserver observer;
  manager_->AddObserver(&observer);
  InitComponentExtension();
  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back("xkb:us:dvorak:eng");
  ids.push_back("xkb:us:colemak:eng");
  EXPECT_TRUE(manager_->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(2U, manager_->GetNumActiveInputMethods());
  // Since all the IDs added avobe are keyboard layouts, Start() should not be
  // called.
  EXPECT_EQ(1, observer.input_method_changed_count_);
  EXPECT_EQ(ids[0], manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", xkeyboard_->last_layout_);
  // Disable Dvorak.
  ids.erase(ids.begin());
  EXPECT_TRUE(manager_->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(1U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ(2, observer.input_method_changed_count_);
  EXPECT_EQ(ids[0],  // colemak
            manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(colemak)", xkeyboard_->last_layout_);
  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestEnableThreeLayouts) {
  // For http://crbug.com/19655#c11 - (9).
  TestObserver observer;
  manager_->AddObserver(&observer);
  InitComponentExtension();
  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back("xkb:us::eng");
  ids.push_back("xkb:us:dvorak:eng");
  ids.push_back("xkb:us:colemak:eng");
  EXPECT_TRUE(manager_->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(3U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ(1, observer.input_method_changed_count_);
  EXPECT_EQ(ids[0], manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);
  // Switch to Dvorak.
  manager_->SwitchToNextInputMethod();
  EXPECT_EQ(2, observer.input_method_changed_count_);
  EXPECT_EQ(ids[1], manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", xkeyboard_->last_layout_);
  // Disable Dvorak.
  ids.erase(ids.begin() + 1);
  EXPECT_TRUE(manager_->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(2U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ(3, observer.input_method_changed_count_);
  EXPECT_EQ(ids[0],  // US Qwerty
            manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);
  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestEnableLayoutAndIme) {
  // For http://crbug.com/19655#c11 - (10).
  TestObserver observer;
  manager_->AddObserver(&observer);
  InitComponentExtension();
  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back("xkb:us:dvorak:eng");
  ids.push_back(kNaclMozcUsId);
  EXPECT_TRUE(manager_->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(1, observer.input_method_changed_count_);
  EXPECT_EQ(ids[0], manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", xkeyboard_->last_layout_);
  // Switch to Mozc
  manager_->SwitchToNextInputMethod();
  EXPECT_EQ(2, observer.input_method_changed_count_);
  EXPECT_EQ(ids[1], manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);
  // Disable Mozc.
  ids.erase(ids.begin() + 1);
  EXPECT_TRUE(manager_->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(1U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ(ids[0], manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", xkeyboard_->last_layout_);
}

TEST_F(InputMethodManagerImplTest, TestEnableLayoutAndIme2) {
  // For http://crbug.com/19655#c11 - (11).
  TestObserver observer;
  manager_->AddObserver(&observer);
  InitComponentExtension();
  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back("xkb:us:dvorak:eng");
  ids.push_back(kNaclMozcUsId);
  EXPECT_TRUE(manager_->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(1, observer.input_method_changed_count_);
  EXPECT_EQ(ids[0], manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", xkeyboard_->last_layout_);

  // Disable Dvorak.
  ids.erase(ids.begin());
  EXPECT_TRUE(manager_->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(1U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ(ids[0],  // Mozc
            manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);
  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestEnableImes) {
  TestObserver observer;
  manager_->AddObserver(&observer);
  InitComponentExtension();
  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back(kExt2Engine1Id);
  ids.push_back("mozc-dv");
  EXPECT_TRUE(manager_->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(1, observer.input_method_changed_count_);
  EXPECT_EQ(ids[0], manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);
  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestEnableUnknownIds) {
  TestObserver observer;
  manager_->AddObserver(&observer);
  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back("xkb:tl::tlh");  // Klingon, which is not supported.
  ids.push_back("unknown-super-cool-ime");
  EXPECT_FALSE(manager_->ReplaceEnabledInputMethods(ids));

  // TODO(yusukes): Should we fall back to the hardware keyboard layout in this
  // case?
  EXPECT_EQ(0, observer.input_method_changed_count_);

  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestEnableLayoutsThenLock) {
  // For http://crbug.com/19655#c11 - (14).
  TestObserver observer;
  manager_->AddObserver(&observer);
  InitComponentExtension();
  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back("xkb:us::eng");
  ids.push_back("xkb:us:dvorak:eng");
  EXPECT_TRUE(manager_->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(2U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ(1, observer.input_method_changed_count_);
  EXPECT_EQ(ids[0], manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);

  // Switch to Dvorak.
  manager_->SwitchToNextInputMethod();
  EXPECT_EQ(2, observer.input_method_changed_count_);
  EXPECT_EQ(ids[1], manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", xkeyboard_->last_layout_);

  // Lock screen
  manager_->SetState(InputMethodManager::STATE_LOCK_SCREEN);
  EXPECT_EQ(2U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ(ids[1],  // still Dvorak
            manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", xkeyboard_->last_layout_);
  // Switch back to Qwerty.
  manager_->SwitchToNextInputMethod();
  EXPECT_EQ(ids[0], manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);

  // Unlock screen. The original state, Dvorak, is restored.
  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  EXPECT_EQ(2U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ(ids[1], manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", xkeyboard_->last_layout_);

  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, SwitchInputMethodTest) {
  // For http://crbug.com/19655#c11 - (15).
  TestObserver observer;
  manager_->AddObserver(&observer);
  InitComponentExtension();
  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back("xkb:us:dvorak:eng");
  ids.push_back(kExt2Engine2Id);
  ids.push_back(kExt2Engine1Id);
  EXPECT_TRUE(manager_->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(3U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ(1, observer.input_method_changed_count_);
  EXPECT_EQ(ids[0], manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", xkeyboard_->last_layout_);

  // Switch to Mozc.
  manager_->SwitchToNextInputMethod();
  EXPECT_EQ(2, observer.input_method_changed_count_);
  EXPECT_EQ(ids[1], manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", xkeyboard_->last_layout_);

  // Lock screen
  manager_->SetState(InputMethodManager::STATE_LOCK_SCREEN);
  EXPECT_EQ(2U, manager_->GetNumActiveInputMethods());  // Qwerty+Dvorak.
  EXPECT_EQ("xkb:us:dvorak:eng",
            manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", xkeyboard_->last_layout_);
  manager_->SwitchToNextInputMethod();
  EXPECT_EQ("xkb:us::eng",  // The hardware keyboard layout.
            manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);

  // Unlock screen. The original state, pinyin-dv, is restored.
  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  EXPECT_EQ(3U, manager_->GetNumActiveInputMethods());  // Dvorak and 2 IMEs.
  EXPECT_EQ(ids[1], manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", xkeyboard_->last_layout_);

  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestXkbSetting) {
  // For http://crbug.com/19655#c11 - (8), step 7-11.
  InitComponentExtension();
  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back("xkb:us:dvorak:eng");
  ids.push_back("xkb:us:colemak:eng");
  ids.push_back(kNaclMozcJpId);
  ids.push_back(kNaclMozcUsId);
  EXPECT_TRUE(manager_->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(4U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ(1, xkeyboard_->set_current_keyboard_layout_by_name_count_);
  // See input_methods.txt for an expected XKB layout name.
  EXPECT_EQ("us(dvorak)", xkeyboard_->last_layout_);
  manager_->SwitchToNextInputMethod();
  EXPECT_EQ(2, xkeyboard_->set_current_keyboard_layout_by_name_count_);
  EXPECT_EQ("us(colemak)", xkeyboard_->last_layout_);
  manager_->SwitchToNextInputMethod();
  EXPECT_EQ(3, xkeyboard_->set_current_keyboard_layout_by_name_count_);
  EXPECT_EQ("jp", xkeyboard_->last_layout_);
  manager_->SwitchToNextInputMethod();
  EXPECT_EQ(4, xkeyboard_->set_current_keyboard_layout_by_name_count_);
  EXPECT_EQ("us", xkeyboard_->last_layout_);
  manager_->SwitchToNextInputMethod();
  EXPECT_EQ(5, xkeyboard_->set_current_keyboard_layout_by_name_count_);
  EXPECT_EQ("us(dvorak)", xkeyboard_->last_layout_);
  // Disable Dvorak.
  ids.erase(ids.begin());
  EXPECT_TRUE(manager_->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(3U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ(6, xkeyboard_->set_current_keyboard_layout_by_name_count_);
  EXPECT_EQ("us(colemak)", xkeyboard_->last_layout_);
}

TEST_F(InputMethodManagerImplTest, TestActivateInputMethodProperty) {
  const std::string kKey = "key";
  InputMethodPropertyList property_list;
  property_list.push_back(InputMethodProperty(kKey, "label", false, false));
  manager_->SetCurrentInputMethodProperties(property_list);

  manager_->ActivateInputMethodProperty(kKey);
  EXPECT_EQ(kKey, mock_engine_handler_->last_activated_property());

  // Key2 is not registered, so activated property should not be changed.
  manager_->ActivateInputMethodProperty("key2");
  EXPECT_EQ(kKey, mock_engine_handler_->last_activated_property());
}

TEST_F(InputMethodManagerImplTest, TestGetCurrentInputMethodProperties) {
  InitComponentExtension();
  EXPECT_TRUE(manager_->GetCurrentInputMethodProperties().empty());

  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back("xkb:us::eng");
  ids.push_back(kNaclMozcUsId);
  EXPECT_TRUE(manager_->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(2U, manager_->GetNumActiveInputMethods());
  EXPECT_TRUE(manager_->GetCurrentInputMethodProperties().empty());
  manager_->ChangeInputMethod(kNaclMozcUsId);

  InputMethodPropertyList current_property_list;
  current_property_list.push_back(InputMethodProperty("key",
                                                      "label",
                                                      false,
                                                      false));
  manager_->SetCurrentInputMethodProperties(current_property_list);

  ASSERT_EQ(1U, manager_->GetCurrentInputMethodProperties().size());
  EXPECT_EQ("key", manager_->GetCurrentInputMethodProperties().at(0).key);

  manager_->ChangeInputMethod("xkb:us::eng");
  EXPECT_TRUE(manager_->GetCurrentInputMethodProperties().empty());
}

TEST_F(InputMethodManagerImplTest, TestGetCurrentInputMethodPropertiesTwoImes) {
  InitComponentExtension();
  EXPECT_TRUE(manager_->GetCurrentInputMethodProperties().empty());

  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back(kNaclMozcUsId);  // Japanese
  ids.push_back(kExt2Engine1Id);  // T-Chinese
  EXPECT_TRUE(manager_->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(2U, manager_->GetNumActiveInputMethods());
  EXPECT_TRUE(manager_->GetCurrentInputMethodProperties().empty());

  InputMethodPropertyList current_property_list;
  current_property_list.push_back(InputMethodProperty("key-mozc",
                                                      "label",
                                                      false,
                                                      false));
  manager_->SetCurrentInputMethodProperties(current_property_list);

  ASSERT_EQ(1U, manager_->GetCurrentInputMethodProperties().size());
  EXPECT_EQ("key-mozc", manager_->GetCurrentInputMethodProperties().at(0).key);

  manager_->ChangeInputMethod(kExt2Engine1Id);
  // Since the IME is changed, the property for mozc Japanese should be hidden.
  EXPECT_TRUE(manager_->GetCurrentInputMethodProperties().empty());

  // Asynchronous property update signal from mozc-chewing.
  current_property_list.clear();
  current_property_list.push_back(InputMethodProperty("key-chewing",
                                                      "label",
                                                      false,
                                                      false));
  manager_->SetCurrentInputMethodProperties(current_property_list);
  ASSERT_EQ(1U, manager_->GetCurrentInputMethodProperties().size());
  EXPECT_EQ("key-chewing",
            manager_->GetCurrentInputMethodProperties().at(0).key);
}

TEST_F(InputMethodManagerImplTest, TestNextInputMethod) {
  TestObserver observer;
  manager_->AddObserver(&observer);
  InitComponentExtension();
  std::vector<std::string> keyboard_layouts;
  keyboard_layouts.push_back("xkb:us::eng");
  // For http://crbug.com/19655#c11 - (1)
  manager_->EnableLoginLayouts("en-US", keyboard_layouts);
  EXPECT_EQ(5U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ("xkb:us::eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);
  manager_->SwitchToNextInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ("xkb:us:intl:eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(intl)", xkeyboard_->last_layout_);
  manager_->SwitchToNextInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ("xkb:us:altgr-intl:eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(altgr-intl)", xkeyboard_->last_layout_);
  manager_->SwitchToNextInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ("xkb:us:dvorak:eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", xkeyboard_->last_layout_);
  manager_->SwitchToNextInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ("xkb:us:colemak:eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(colemak)", xkeyboard_->last_layout_);
  manager_->SwitchToNextInputMethod();
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ("xkb:us::eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);

  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestPreviousInputMethod) {
  TestObserver observer;
  manager_->AddObserver(&observer);
  InitComponentExtension();

  ui::Accelerator keydown_accelerator(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN);
  keydown_accelerator.set_type(ui::ET_KEY_PRESSED);
  ui::Accelerator keyup_accelerator(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN);
  keyup_accelerator.set_type(ui::ET_KEY_RELEASED);

  std::vector<std::string> keyboard_layouts;
  keyboard_layouts.push_back("xkb:us::eng");
  manager_->EnableLoginLayouts("en-US", keyboard_layouts);
  EXPECT_EQ(5U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ("xkb:us::eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);
  EXPECT_TRUE(manager_->SwitchToNextInputMethod());
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ("xkb:us:intl:eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(intl)", xkeyboard_->last_layout_);
  EXPECT_TRUE(manager_->SwitchToPreviousInputMethod(keydown_accelerator));
  EXPECT_TRUE(manager_->SwitchToPreviousInputMethod(keyup_accelerator));
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ("xkb:us::eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);
  EXPECT_TRUE(manager_->SwitchToPreviousInputMethod(keydown_accelerator));
  EXPECT_TRUE(manager_->SwitchToPreviousInputMethod(keyup_accelerator));
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ("xkb:us:intl:eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(intl)", xkeyboard_->last_layout_);
  EXPECT_TRUE(manager_->SwitchToPreviousInputMethod(keydown_accelerator));
  EXPECT_TRUE(manager_->SwitchToPreviousInputMethod(keyup_accelerator));
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ("xkb:us::eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);
  EXPECT_TRUE(manager_->SwitchToNextInputMethod());
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ("xkb:us:intl:eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(intl)", xkeyboard_->last_layout_);
  EXPECT_TRUE(manager_->SwitchToNextInputMethod());
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ("xkb:us:altgr-intl:eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(altgr-intl)", xkeyboard_->last_layout_);
  EXPECT_TRUE(manager_->SwitchToPreviousInputMethod(keydown_accelerator));
  EXPECT_TRUE(manager_->SwitchToPreviousInputMethod(keyup_accelerator));
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ("xkb:us:intl:eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(intl)", xkeyboard_->last_layout_);
  EXPECT_TRUE(manager_->SwitchToPreviousInputMethod(keydown_accelerator));
  EXPECT_TRUE(manager_->SwitchToPreviousInputMethod(keyup_accelerator));
  EXPECT_TRUE(observer.last_show_message_);
  EXPECT_EQ("xkb:us:altgr-intl:eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(altgr-intl)", xkeyboard_->last_layout_);

  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest,
       TestSwitchToPreviousInputMethodForOneActiveInputMethod) {
  TestObserver observer;
  manager_->AddObserver(&observer);
  InitComponentExtension();

  ui::Accelerator keydown_accelerator(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN);
  keydown_accelerator.set_type(ui::ET_KEY_PRESSED);
  ui::Accelerator keyup_accelerator(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN);
  keyup_accelerator.set_type(ui::ET_KEY_RELEASED);

  std::vector<std::string> ids;
  ids.push_back("xkb:us:dvorak:eng");
  EXPECT_TRUE(manager_->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(1U, manager_->GetNumActiveInputMethods());

  // Ctrl+Space accelerator should not be consumed if there is only one active
  // input method.
  EXPECT_FALSE(manager_->SwitchToPreviousInputMethod(keydown_accelerator));
  EXPECT_FALSE(manager_->SwitchToPreviousInputMethod(keyup_accelerator));

  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestSwitchInputMethodWithUsLayouts) {
  TestObserver observer;
  manager_->AddObserver(&observer);
  InitComponentExtension();
  std::vector<std::string> keyboard_layouts;
  keyboard_layouts.push_back("xkb:us::eng");
  manager_->EnableLoginLayouts("en-US", keyboard_layouts);
  EXPECT_EQ(5U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ("xkb:us::eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);

  // Henkan, Muhenkan, ZenkakuHankaku should be ignored when no Japanese IMEs
  // and keyboards are enabled.
  EXPECT_FALSE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_CONVERT, ui::EF_NONE)));
  EXPECT_FALSE(observer.last_show_message_);
  EXPECT_EQ("xkb:us::eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);
  EXPECT_FALSE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_NONCONVERT, ui::EF_NONE)));
  EXPECT_EQ("xkb:us::eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);
  EXPECT_FALSE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_DBE_SBCSCHAR, ui::EF_NONE)));
  EXPECT_EQ("xkb:us::eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);
  EXPECT_FALSE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_DBE_DBCSCHAR, ui::EF_NONE)));
  EXPECT_EQ("xkb:us::eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);

  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest, TestSwitchInputMethodWithJpLayout) {
  // Enable "xkb:jp::jpn" and press Muhenkan/ZenkakuHankaku.
  InitComponentExtension();

  ui::Accelerator keydown_accelerator(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN);
  keydown_accelerator.set_type(ui::ET_KEY_PRESSED);
  ui::Accelerator keyup_accelerator(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN);
  keyup_accelerator.set_type(ui::ET_KEY_RELEASED);

  std::vector<std::string> keyboard_layouts;
  keyboard_layouts.push_back("xkb:us::eng");
  manager_->EnableLoginLayouts("ja", keyboard_layouts);
  EXPECT_EQ(2U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ("xkb:us::eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);
  EXPECT_TRUE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_NONCONVERT, ui::EF_NONE)));
  EXPECT_EQ("xkb:jp::jpn", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("jp", xkeyboard_->last_layout_);
  EXPECT_TRUE(manager_->SwitchToPreviousInputMethod(keydown_accelerator));
  EXPECT_TRUE(manager_->SwitchToPreviousInputMethod(keyup_accelerator));
  EXPECT_EQ("xkb:us::eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);
  EXPECT_TRUE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_DBE_SBCSCHAR, ui::EF_NONE)));
  EXPECT_EQ("xkb:jp::jpn", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("jp", xkeyboard_->last_layout_);
  EXPECT_TRUE(manager_->SwitchToPreviousInputMethod(keydown_accelerator));
  EXPECT_TRUE(manager_->SwitchToPreviousInputMethod(keyup_accelerator));
  EXPECT_EQ("xkb:us::eng", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);
  EXPECT_TRUE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_DBE_DBCSCHAR, ui::EF_NONE)));
  EXPECT_EQ("xkb:jp::jpn", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("jp", xkeyboard_->last_layout_);
}

TEST_F(InputMethodManagerImplTest, TestSwitchInputMethodWithJpIme) {
  InitComponentExtension();
  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back("xkb:jp::jpn");
  ids.push_back(kNaclMozcJpId);
  EXPECT_TRUE(manager_->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ("xkb:jp::jpn", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("jp", xkeyboard_->last_layout_);
  EXPECT_TRUE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_DBE_DBCSCHAR, ui::EF_NONE)));
  EXPECT_EQ(kNaclMozcJpId, manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("jp", xkeyboard_->last_layout_);
  EXPECT_TRUE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_DBE_DBCSCHAR, ui::EF_NONE)));
  EXPECT_EQ("xkb:jp::jpn", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("jp", xkeyboard_->last_layout_);
  EXPECT_TRUE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_CONVERT, ui::EF_NONE)));
  EXPECT_EQ(kNaclMozcJpId, manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("jp", xkeyboard_->last_layout_);
  EXPECT_TRUE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_CONVERT, ui::EF_NONE)));
  EXPECT_EQ(kNaclMozcJpId, manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("jp", xkeyboard_->last_layout_);
  EXPECT_TRUE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_NONCONVERT, ui::EF_NONE)));
  EXPECT_EQ("xkb:jp::jpn", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("jp", xkeyboard_->last_layout_);
  EXPECT_TRUE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_NONCONVERT, ui::EF_NONE)));
  EXPECT_EQ("xkb:jp::jpn", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("jp", xkeyboard_->last_layout_);

  // Add Dvorak.
  ids.push_back("xkb:us:dvorak:eng");
  EXPECT_TRUE(manager_->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ("xkb:jp::jpn", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("jp", xkeyboard_->last_layout_);
  EXPECT_TRUE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_DBE_SBCSCHAR, ui::EF_NONE)));
  EXPECT_EQ(kNaclMozcJpId, manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("jp", xkeyboard_->last_layout_);
  EXPECT_TRUE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_DBE_SBCSCHAR, ui::EF_NONE)));
  EXPECT_EQ("xkb:jp::jpn", manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("jp", xkeyboard_->last_layout_);
}

TEST_F(InputMethodManagerImplTest, TestAddRemoveExtensionInputMethods) {
  TestObserver observer;
  manager_->AddObserver(&observer);
  InitComponentExtension();
  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back("xkb:us:dvorak:eng");
  EXPECT_TRUE(manager_->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(1U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ(1, observer.input_method_changed_count_);
  EXPECT_EQ(ids[0],
            manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", xkeyboard_->last_layout_);

  // Add two Extension IMEs.
  std::vector<std::string> layouts;
  layouts.push_back("us");
  std::vector<std::string> languages;
  languages.push_back("en-US");

  const std::string ext1_id =
      extension_ime_util::GetInputMethodID("deadbeef", "engine_id");
  const InputMethodDescriptor descriptor1(ext1_id,
                                          "deadbeef input method",
                                          "DB",
                                          layouts,
                                          languages,
                                          false,  // is_login_keyboard
                                          GURL(),
                                          GURL());
  MockInputMethodEngine engine(descriptor1);
  manager_->AddInputMethodExtension(ext1_id, &engine);

  // Extension IMEs are not enabled by default.
  EXPECT_EQ(1U, manager_->GetNumActiveInputMethods());

  std::vector<std::string> extension_ime_ids;
  extension_ime_ids.push_back(ext1_id);
  manager_->SetEnabledExtensionImes(&extension_ime_ids);
  EXPECT_EQ(2U, manager_->GetNumActiveInputMethods());

  {
    scoped_ptr<InputMethodDescriptors> methods(
        manager_->GetActiveInputMethods());
    ASSERT_EQ(2U, methods->size());
    // Ext IMEs should be at the end of the list.
    EXPECT_EQ(ext1_id, methods->at(1).id());
  }

  const std::string ext2_id =
      extension_ime_util::GetInputMethodID("cafebabe", "engine_id");
  const InputMethodDescriptor descriptor2(ext2_id,
                                          "cafebabe input method",
                                          "CB",
                                          layouts,
                                          languages,
                                          false,  // is_login_keyboard
                                          GURL(),
                                          GURL());
  MockInputMethodEngine engine2(descriptor2);
  manager_->AddInputMethodExtension(ext2_id, &engine2);
  EXPECT_EQ(2U, manager_->GetNumActiveInputMethods());

  extension_ime_ids.push_back(ext2_id);
  manager_->SetEnabledExtensionImes(&extension_ime_ids);
  EXPECT_EQ(3U, manager_->GetNumActiveInputMethods());
  {
    scoped_ptr<InputMethodDescriptors> methods(
        manager_->GetActiveInputMethods());
    ASSERT_EQ(3U, methods->size());
    // Ext IMEs should be at the end of the list.
    EXPECT_EQ(ext1_id, methods->at(1).id());
    EXPECT_EQ(ext2_id, methods->at(2).id());
  }

  // Remove them.
  manager_->RemoveInputMethodExtension(ext1_id);
  EXPECT_EQ(2U, manager_->GetNumActiveInputMethods());
  manager_->RemoveInputMethodExtension(ext2_id);
  EXPECT_EQ(1U, manager_->GetNumActiveInputMethods());
}

TEST_F(InputMethodManagerImplTest, TestAddExtensionInputThenLockScreen) {
  TestObserver observer;
  InitComponentExtension();
  manager_->AddObserver(&observer);
  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back("xkb:us::eng");
  EXPECT_TRUE(manager_->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(1U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ(1, observer.input_method_changed_count_);
  EXPECT_EQ(ids[0], manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);

  // Add an Extension IME.
  std::vector<std::string> layouts;
  layouts.push_back("us(dvorak)");
  std::vector<std::string> languages;
  languages.push_back("en-US");

  const std::string ext_id =
      extension_ime_util::GetInputMethodID("deadbeef", "engine_id");
  const InputMethodDescriptor descriptor(ext_id,
                                         "deadbeef input method",
                                         "DB",
                                         layouts,
                                         languages,
                                         false,  // is_login_keyboard
                                         GURL(),
                                         GURL());
  MockInputMethodEngine engine(descriptor);
  manager_->AddInputMethodExtension(ext_id, &engine);

  // Extension IME is not enabled by default.
  EXPECT_EQ(1U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ(1, observer.input_method_changed_count_);

  std::vector<std::string> extension_ime_ids;
  extension_ime_ids.push_back(ext_id);
  manager_->SetEnabledExtensionImes(&extension_ime_ids);
  EXPECT_EQ(2U, manager_->GetNumActiveInputMethods());

  // Switch to the IME.
  manager_->SwitchToNextInputMethod();
  EXPECT_EQ(3, observer.input_method_changed_count_);
  EXPECT_EQ(ext_id, manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", xkeyboard_->last_layout_);

  // Lock the screen. This is for crosbug.com/27049.
  manager_->SetState(InputMethodManager::STATE_LOCK_SCREEN);
  EXPECT_EQ(1U, manager_->GetNumActiveInputMethods());  // Qwerty. No Ext. IME
  EXPECT_EQ("xkb:us::eng",
            manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us", xkeyboard_->last_layout_);

  // Unlock the screen.
  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  EXPECT_EQ(2U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ(ext_id, manager_->GetCurrentInputMethod().id());
  EXPECT_EQ("us(dvorak)", xkeyboard_->last_layout_);
  {
    // This is for crosbug.com/27052.
    scoped_ptr<InputMethodDescriptors> methods(
        manager_->GetActiveInputMethods());
    ASSERT_EQ(2U, methods->size());
    // Ext. IMEs should be at the end of the list.
    EXPECT_EQ(ext_id, methods->at(1).id());
  }
  manager_->RemoveObserver(&observer);
}

TEST_F(InputMethodManagerImplTest,
       ChangeInputMethodBeforeComponentExtensionInitialization_OneIME) {
  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back(kNaclMozcUsId);
  EXPECT_TRUE(manager_->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(1U, manager_->GetNumActiveInputMethods());
  manager_->ChangeInputMethod(kNaclMozcUsId);

  InitComponentExtension();
  EXPECT_EQ(kNaclMozcUsId, manager_->GetCurrentInputMethod().id());
}

TEST_F(InputMethodManagerImplTest,
       ChangeInputMethodBeforeComponentExtensionInitialization_TwoIME) {
  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  std::vector<std::string> ids;
  ids.push_back(kNaclMozcUsId);
  ids.push_back(kNaclMozcJpId);
  EXPECT_TRUE(manager_->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(2U, manager_->GetNumActiveInputMethods());
  manager_->ChangeInputMethod(kNaclMozcUsId);
  manager_->ChangeInputMethod(kNaclMozcJpId);

  InitComponentExtension();
  EXPECT_EQ(kNaclMozcJpId, manager_->GetCurrentInputMethod().id());
}

TEST_F(InputMethodManagerImplTest,
       ChangeInputMethodBeforeComponentExtensionInitialization_CompOneIME) {
  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  const std::string ext_id = extension_ime_util::GetComponentInputMethodID(
      ime_list_[0].id,
      ime_list_[0].engines[0].engine_id);
  std::vector<std::string> ids;
  ids.push_back(ext_id);
  EXPECT_TRUE(manager_->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(1U, manager_->GetNumActiveInputMethods());
  manager_->ChangeInputMethod(ext_id);

  InitComponentExtension();
  EXPECT_EQ(ext_id, manager_->GetCurrentInputMethod().id());
}

TEST_F(InputMethodManagerImplTest,
       ChangeInputMethodBeforeComponentExtensionInitialization_CompTwoIME) {
  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  const std::string ext_id1 = extension_ime_util::GetComponentInputMethodID(
      ime_list_[0].id,
      ime_list_[0].engines[0].engine_id);
  const std::string ext_id2 = extension_ime_util::GetComponentInputMethodID(
      ime_list_[1].id,
      ime_list_[1].engines[0].engine_id);
  std::vector<std::string> ids;
  ids.push_back(ext_id1);
  ids.push_back(ext_id2);
  EXPECT_TRUE(manager_->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(2U, manager_->GetNumActiveInputMethods());
  manager_->ChangeInputMethod(ext_id1);
  manager_->ChangeInputMethod(ext_id2);

  InitComponentExtension();
  EXPECT_EQ(ext_id2, manager_->GetCurrentInputMethod().id());
}

TEST_F(InputMethodManagerImplTest,
       ChangeInputMethod_ComponenteExtensionOneIME) {
  InitComponentExtension();
  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  const std::string ext_id = extension_ime_util::GetComponentInputMethodID(
      ime_list_[0].id,
      ime_list_[0].engines[0].engine_id);
  std::vector<std::string> ids;
  ids.push_back(ext_id);
  EXPECT_TRUE(manager_->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(1U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ(ext_id, manager_->GetCurrentInputMethod().id());
}

TEST_F(InputMethodManagerImplTest,
       ChangeInputMethod_ComponenteExtensionTwoIME) {
  InitComponentExtension();
  manager_->SetState(InputMethodManager::STATE_BROWSER_SCREEN);
  const std::string ext_id1 = extension_ime_util::GetComponentInputMethodID(
      ime_list_[0].id,
      ime_list_[0].engines[0].engine_id);
  const std::string ext_id2 = extension_ime_util::GetComponentInputMethodID(
      ime_list_[1].id,
      ime_list_[1].engines[0].engine_id);
  std::vector<std::string> ids;
  ids.push_back(ext_id1);
  ids.push_back(ext_id2);
  EXPECT_TRUE(manager_->ReplaceEnabledInputMethods(ids));
  EXPECT_EQ(2U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ(ext_id1, manager_->GetCurrentInputMethod().id());
  manager_->ChangeInputMethod(ext_id2);
  EXPECT_EQ(ext_id2, manager_->GetCurrentInputMethod().id());
}

}  // namespace input_method
}  // namespace chromeos
