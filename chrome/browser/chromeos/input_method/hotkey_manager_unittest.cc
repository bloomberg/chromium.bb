// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/hotkey_manager.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <X11/X.h>  // ShiftMask, ControlMask, etc.
#include <X11/Xutil.h>  // for XK_* macros.

namespace chromeos {
namespace input_method {

class TestableHotkeyManager : public HotkeyManager {
 public:
  // Change access rights.
  using HotkeyManager::FilterKeyEventInternal;
  using HotkeyManager::NormalizeModifiers;
  using HotkeyManager::IsModifier;
  using HotkeyManager::kKeyReleaseMask;
  using HotkeyManager::kNoEvent;
  using HotkeyManager::kFiltered;
};

namespace {

const int kPreviousEngineId = 0;
const int kNextEngineId = 1;

TestableHotkeyManager* CreateManager() {
  TestableHotkeyManager* manager = new TestableHotkeyManager;
  // Add Ctrl+space.
  manager->AddHotkey(kPreviousEngineId,
                     XK_space,
                     ControlMask,
                     true /* trigger on key press */);

  // Add Alt+Shift. For this hotkey, we use "trigger on key release" so that
  // the current IME is not changed on pressing Alt+Shift+something.
  manager->AddHotkey(kNextEngineId,
                     XK_Shift_L,
                     ShiftMask | Mod1Mask,
                     false /* trigger on key release. see the comment above */);
  manager->AddHotkey(kNextEngineId,
                     XK_Shift_R,
                     ShiftMask | Mod1Mask,
                     false);
  manager->AddHotkey(kNextEngineId,
                     XK_Meta_L,
                     ShiftMask | Mod1Mask,
                     false);
  manager->AddHotkey(kNextEngineId,
                     XK_Meta_R,
                     ShiftMask | Mod1Mask,
                     false);
  return manager;
}

}  // namespace

TEST(HotkeyManagerTest, TestIsModifier) {
  TestableHotkeyManager manager;
  EXPECT_TRUE(manager.IsModifier(XK_Control_L));
  EXPECT_TRUE(manager.IsModifier(XK_Control_R));
  EXPECT_TRUE(manager.IsModifier(XK_Shift_L));
  EXPECT_TRUE(manager.IsModifier(XK_Shift_R));
  EXPECT_TRUE(manager.IsModifier(XK_Alt_L));
  EXPECT_TRUE(manager.IsModifier(XK_Alt_R));
  EXPECT_TRUE(manager.IsModifier(XK_Meta_L));
  EXPECT_TRUE(manager.IsModifier(XK_Meta_R));
  EXPECT_TRUE(manager.IsModifier(XK_Super_L));
  EXPECT_TRUE(manager.IsModifier(XK_Super_R));
  EXPECT_TRUE(manager.IsModifier(XK_Hyper_L));
  EXPECT_TRUE(manager.IsModifier(XK_Hyper_R));
  EXPECT_FALSE(manager.IsModifier(XK_a));
  EXPECT_FALSE(manager.IsModifier(XK_A));
  EXPECT_FALSE(manager.IsModifier(XK_Tab));
}

TEST(HotkeyManagerTest, TestNormalizeModifiers) {
  static const uint32 kReleaseMask = TestableHotkeyManager::kKeyReleaseMask;

  TestableHotkeyManager manager;
  EXPECT_EQ(ControlMask | Mod1Mask | 0x0U,
            manager.NormalizeModifiers(XK_Control_L, Mod1Mask, true));
  EXPECT_EQ(ControlMask | Mod1Mask | kReleaseMask,
            manager.NormalizeModifiers(XK_Control_R, Mod1Mask, false));
  EXPECT_EQ(ShiftMask | Mod1Mask | 0x0U,
            manager.NormalizeModifiers(XK_Shift_L, Mod1Mask, true));
  EXPECT_EQ(ShiftMask | Mod1Mask | kReleaseMask,
            manager.NormalizeModifiers(XK_Shift_R, Mod1Mask, false));
  EXPECT_EQ(Mod1Mask | ShiftMask | 0x0U,
            manager.NormalizeModifiers(XK_Alt_L, ShiftMask, true));
  EXPECT_EQ(Mod1Mask | ShiftMask | kReleaseMask,
            manager.NormalizeModifiers(XK_Alt_R, ShiftMask, false));
  EXPECT_EQ(Mod1Mask | ShiftMask | 0x0U,
            manager.NormalizeModifiers(XK_Meta_L, ShiftMask, true));
  EXPECT_EQ(Mod1Mask | ShiftMask | kReleaseMask,
            manager.NormalizeModifiers(XK_Meta_R, ShiftMask, false));
  EXPECT_EQ(ShiftMask | Mod2Mask | 0x0U,
            manager.NormalizeModifiers(XK_Super_L, ShiftMask, true));
  EXPECT_EQ(Mod1Mask | Mod2Mask | kReleaseMask,
            manager.NormalizeModifiers(XK_Super_R, Mod1Mask, false));
  EXPECT_EQ(ShiftMask | Mod3Mask | kReleaseMask,
            manager.NormalizeModifiers(XK_Hyper_L, ShiftMask, false));
  EXPECT_EQ(Mod1Mask | Mod3Mask | 0x0U,
            manager.NormalizeModifiers(XK_Hyper_R, Mod1Mask, true));

  // Test non modifier keys like XK_a.
  EXPECT_EQ(Mod1Mask | 0x0U,
            manager.NormalizeModifiers(XK_a, Mod1Mask, true));
  EXPECT_EQ(Mod1Mask | kReleaseMask,
            manager.NormalizeModifiers(XK_b, Mod1Mask, false));
  EXPECT_EQ(Mod1Mask | ControlMask | 0x0U,
            manager.NormalizeModifiers(
                XK_C, Mod1Mask | ControlMask, true));
  EXPECT_EQ(Mod1Mask | ControlMask | kReleaseMask,
            manager.NormalizeModifiers(
                XK_D, Mod1Mask | ControlMask, false));
}

TEST(HotkeyManagerTest, TestAddBasic) {
  TestableHotkeyManager manager;
  EXPECT_TRUE(manager.AddHotkey(0, XK_space, ControlMask, true));
  // Adding another hotkey with the same ID should be allowed.
  EXPECT_TRUE(manager.AddHotkey(0, XK_space, Mod1Mask, true));
  // It's not allowed to add the same hotkey twice.
  EXPECT_FALSE(manager.AddHotkey(1, XK_space, ControlMask, true));
  // It's not allowed to use kNoEvent as an event id.
  EXPECT_FALSE(manager.AddHotkey(
      TestableHotkeyManager::kNoEvent, XK_A, Mod1Mask, true));
  // It's not allowed to use negative numbers as an event id.
  EXPECT_FALSE(manager.AddHotkey(-2, XK_B, Mod1Mask, true));
  EXPECT_FALSE(manager.AddHotkey(-123, XK_C, Mod1Mask, false));
}

TEST(HotkeyManagerTest, TestControlSpace) {
  TestableHotkeyManager manager;
  EXPECT_TRUE(manager.AddHotkey(0, XK_space, ControlMask, true));
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager.FilterKeyEventInternal(
      XK_Control_L, 0x0, true));
  EXPECT_EQ(0, manager.FilterKeyEventInternal(XK_space, ControlMask, true));
  EXPECT_EQ(TestableHotkeyManager::kFiltered, manager.FilterKeyEventInternal(
      XK_space, ControlMask, false));
  EXPECT_EQ(TestableHotkeyManager::kFiltered, manager.FilterKeyEventInternal(
      XK_Control_L, ControlMask, false));
}

TEST(HotkeyManagerTest, TestControlSpaceTwice) {
  TestableHotkeyManager manager;
  EXPECT_TRUE(manager.AddHotkey(0, XK_space, ControlMask, true));
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager.FilterKeyEventInternal(
      XK_Control_L, 0x0, true));
  EXPECT_EQ(0, manager.FilterKeyEventInternal(XK_space, ControlMask, true));
  EXPECT_EQ(TestableHotkeyManager::kFiltered, manager.FilterKeyEventInternal(
      XK_space, ControlMask, false));
  EXPECT_EQ(0, manager.FilterKeyEventInternal(XK_space, ControlMask, true));
  EXPECT_EQ(TestableHotkeyManager::kFiltered, manager.FilterKeyEventInternal(
      XK_space, ControlMask, false));
  EXPECT_EQ(TestableHotkeyManager::kFiltered, manager.FilterKeyEventInternal(
      XK_Control_L, ControlMask, false));
}

TEST(HotkeyManagerTest, TestControlSpaceTwiceTriggerOnRelease) {
  TestableHotkeyManager manager;
  EXPECT_TRUE(manager.AddHotkey(0, XK_space, ControlMask, false));
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager.FilterKeyEventInternal(
      XK_Control_L, 0x0, true));
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager.FilterKeyEventInternal(
      XK_space, ControlMask, true));
  EXPECT_EQ(0, manager.FilterKeyEventInternal(XK_space, ControlMask, false));
  EXPECT_EQ(TestableHotkeyManager::kFiltered, manager.FilterKeyEventInternal(
      XK_Control_L, ControlMask, false));
}

TEST(HotkeyManagerTest, TestAddTwoHotkeys) {
  TestableHotkeyManager manager;
  EXPECT_TRUE(manager.AddHotkey(0, XK_a, 0x0, true));
  EXPECT_TRUE(manager.AddHotkey(1, XK_b, 0x0, false));
  EXPECT_EQ(0, manager.FilterKeyEventInternal(XK_a, 0x0, true));
  EXPECT_EQ(TestableHotkeyManager::kFiltered, manager.FilterKeyEventInternal(
      XK_a, 0x0, false));
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager.FilterKeyEventInternal(
      XK_b, 0x0, true));
  EXPECT_EQ(1, manager.FilterKeyEventInternal(XK_b, 0x0, false));
  EXPECT_EQ(0, manager.FilterKeyEventInternal(XK_a, 0x0, true));
  EXPECT_EQ(TestableHotkeyManager::kFiltered, manager.FilterKeyEventInternal(
      XK_a, 0x0, false));
}

TEST(HotkeyManagerTest, TestRemoveHotkey1) {
  TestableHotkeyManager manager;
  EXPECT_TRUE(manager.AddHotkey(0, XK_a, 0x0, true));
  EXPECT_TRUE(manager.AddHotkey(1, XK_b, 0x0, false));
  EXPECT_EQ(0, manager.FilterKeyEventInternal(XK_a, 0x0, true));
  EXPECT_EQ(TestableHotkeyManager::kFiltered, manager.FilterKeyEventInternal(
      XK_a, 0x0, false));

  // Remove the second hotkey and confirm that it is actually removed.
  EXPECT_TRUE(manager.RemoveHotkey(1));
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager.FilterKeyEventInternal(
      XK_b, 0x0, true));
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager.FilterKeyEventInternal(
      XK_b, 0x0, false));
  // Remove the first one as well.
  EXPECT_TRUE(manager.RemoveHotkey(0));
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager.FilterKeyEventInternal(
      XK_a, 0x0, true));
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager.FilterKeyEventInternal(
      XK_a, 0x0, false));

  // Can't remove a hotkey twice.
  EXPECT_FALSE(manager.RemoveHotkey(0));
  EXPECT_FALSE(manager.RemoveHotkey(1));
  // "2" is not registered yet.
  EXPECT_FALSE(manager.RemoveHotkey(2));
}

TEST(HotkeyManagerTest, TestRemoveHotkey2) {
  TestableHotkeyManager manager;
  // Add two hotkeys that have the same id "0".
  EXPECT_TRUE(manager.AddHotkey(0, XK_a, 0x0, true));
  EXPECT_TRUE(manager.AddHotkey(0, XK_b, 0x0, false));
  EXPECT_EQ(0, manager.FilterKeyEventInternal(XK_a, 0x0, true));
  EXPECT_EQ(TestableHotkeyManager::kFiltered, manager.FilterKeyEventInternal(
      XK_a, 0x0, false));

  // Remove the hotkey and confirm that it is actually removed.
  EXPECT_TRUE(manager.RemoveHotkey(0));
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager.FilterKeyEventInternal(
      XK_b, 0x0, true));
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager.FilterKeyEventInternal(
      XK_b, 0x0, false));
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager.FilterKeyEventInternal(
      XK_a, 0x0, true));
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager.FilterKeyEventInternal(
      XK_a, 0x0, false));

  // Can't remove a hotkey twice.
  EXPECT_FALSE(manager.RemoveHotkey(0));
  // "1" is not registered yet.
  EXPECT_FALSE(manager.RemoveHotkey(1));
}

// Press Shift, then press Alt. Release Alt first.
TEST(HotkeyManagerTest, TestAltShift1) {
  scoped_ptr<TestableHotkeyManager> manager(CreateManager());
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager->FilterKeyEventInternal(
      XK_Shift_L, 0x0, true));
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager->FilterKeyEventInternal(
      XK_Meta_L, ShiftMask, true));
  EXPECT_EQ(kNextEngineId, manager->FilterKeyEventInternal(
      XK_Meta_L, ShiftMask | Mod1Mask, false));
  EXPECT_EQ(TestableHotkeyManager::kFiltered, manager->FilterKeyEventInternal(
      XK_Shift_L, ShiftMask, false));
}

// Press Shift, then press Alt. Release Shift first.
TEST(HotkeyManagerTest, TestAltShift2) {
  scoped_ptr<TestableHotkeyManager> manager(CreateManager());
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager->FilterKeyEventInternal(
      XK_Shift_L, 0x0, true));
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager->FilterKeyEventInternal(
      XK_Meta_L, ShiftMask, true));
  EXPECT_EQ(kNextEngineId, manager->FilterKeyEventInternal(
      XK_Shift_L, ShiftMask | Mod1Mask, false));
  EXPECT_EQ(TestableHotkeyManager::kFiltered, manager->FilterKeyEventInternal(
      XK_Alt_L, Mod1Mask, false));
}

// Press Alt, then press Shift. Release Shift first.
TEST(HotkeyManagerTest, TestAltShift3) {
  scoped_ptr<TestableHotkeyManager> manager(CreateManager());
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager->FilterKeyEventInternal(
      XK_Alt_L, 0x0, true));
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager->FilterKeyEventInternal(
      XK_Shift_L, Mod1Mask, true));
  EXPECT_EQ(kNextEngineId, manager->FilterKeyEventInternal(
      XK_Shift_L, Mod1Mask | ShiftMask, false));
  EXPECT_EQ(TestableHotkeyManager::kFiltered, manager->FilterKeyEventInternal(
      XK_Alt_L, Mod1Mask, false));
}

// Press Alt, then press Shift. Release Alt first.
TEST(HotkeyManagerTest, TestAltShift4) {
  scoped_ptr<TestableHotkeyManager> manager(CreateManager());
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager->FilterKeyEventInternal(
      XK_Alt_L, 0x0, true));
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager->FilterKeyEventInternal(
      XK_Shift_L, Mod1Mask, true));
  EXPECT_EQ(kNextEngineId, manager->FilterKeyEventInternal(
      XK_Meta_L, Mod1Mask | ShiftMask, false));
  EXPECT_EQ(TestableHotkeyManager::kFiltered, manager->FilterKeyEventInternal(
      XK_Shift_L, ShiftMask, false));
}

// Press right Shift, then press right Alt. Release Alt first.
TEST(HotkeyManagerTest, TestAltShift5) {
  scoped_ptr<TestableHotkeyManager> manager(CreateManager());
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager->FilterKeyEventInternal(
      XK_Shift_R, 0x0, true));
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager->FilterKeyEventInternal(
      XK_Meta_R, ShiftMask, true));
  EXPECT_EQ(kNextEngineId, manager->FilterKeyEventInternal(
      XK_Meta_R, ShiftMask | Mod1Mask, false));
  EXPECT_EQ(TestableHotkeyManager::kFiltered, manager->FilterKeyEventInternal(
      XK_Shift_R, ShiftMask, false));
}

// Press right Shift, then press right Alt. Release Shift first.
TEST(HotkeyManagerTest, TestAltShift6) {
  scoped_ptr<TestableHotkeyManager> manager(CreateManager());
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager->FilterKeyEventInternal(
      XK_Shift_R, 0x0, true));
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager->FilterKeyEventInternal(
      XK_Meta_R, ShiftMask, true));
  EXPECT_EQ(kNextEngineId, manager->FilterKeyEventInternal(
      XK_Shift_R, ShiftMask | Mod1Mask, false));
  EXPECT_EQ(TestableHotkeyManager::kFiltered, manager->FilterKeyEventInternal(
      XK_Alt_R, Mod1Mask, false));
}

// Press right Alt, then press right Shift. Release Shift first.
TEST(HotkeyManagerTest, TestAltShift7) {
  scoped_ptr<TestableHotkeyManager> manager(CreateManager());
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager->FilterKeyEventInternal(
      XK_Alt_R, 0x0, true));
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager->FilterKeyEventInternal(
      XK_Shift_R, Mod1Mask, true));
  EXPECT_EQ(kNextEngineId, manager->FilterKeyEventInternal(
      XK_Shift_R, Mod1Mask | ShiftMask, false));
  EXPECT_EQ(TestableHotkeyManager::kFiltered, manager->FilterKeyEventInternal(
      XK_Alt_R, Mod1Mask, false));
}

// Press right Alt, then press right Shift. Release Alt first.
TEST(HotkeyManagerTest, TestAltShift8) {
  scoped_ptr<TestableHotkeyManager> manager(CreateManager());
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager->FilterKeyEventInternal(
      XK_Alt_R, 0x0, true));
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager->FilterKeyEventInternal(
      XK_Shift_R, Mod1Mask, true));
  EXPECT_EQ(kNextEngineId, manager->FilterKeyEventInternal(
      XK_Meta_R, Mod1Mask | ShiftMask, false));
  EXPECT_EQ(TestableHotkeyManager::kFiltered, manager->FilterKeyEventInternal(
      XK_Shift_R, ShiftMask, false));
}

// Press left Shift, then press right Alt. Release Shift first.
TEST(HotkeyManagerTest, TestAltShift9) {
  scoped_ptr<TestableHotkeyManager> manager(CreateManager());
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager->FilterKeyEventInternal(
      XK_Shift_L, 0x0, true));
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager->FilterKeyEventInternal(
      XK_Meta_R, ShiftMask, true));
  EXPECT_EQ(kNextEngineId, manager->FilterKeyEventInternal(
      XK_Shift_L, ShiftMask | Mod1Mask, false));
  EXPECT_EQ(TestableHotkeyManager::kFiltered, manager->FilterKeyEventInternal(
      XK_Alt_R, Mod1Mask, false));
}

// Press left Alt, then press right Shift. Release Alt first.
TEST(HotkeyManagerTest, TestAltShift10) {
  scoped_ptr<TestableHotkeyManager> manager(CreateManager());
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager->FilterKeyEventInternal(
      XK_Alt_L, 0x0, true));
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager->FilterKeyEventInternal(
      XK_Shift_R, Mod1Mask, true));
  EXPECT_EQ(kNextEngineId, manager->FilterKeyEventInternal(
      XK_Meta_L, Mod1Mask | ShiftMask, false));
  EXPECT_EQ(TestableHotkeyManager::kFiltered, manager->FilterKeyEventInternal(
      XK_Shift_R, ShiftMask, false));
}

// Press Shift, then press Alt, then press a. In this case, kNextEngineId event
// should NOT be triggered. See the comment in CreateManager().
TEST(HotkeyManagerTest, TestAltShiftA) {
  scoped_ptr<TestableHotkeyManager> manager(CreateManager());
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager->FilterKeyEventInternal(
      XK_Shift_L, 0x0, true));
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager->FilterKeyEventInternal(
      XK_Meta_L, ShiftMask, true));
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager->FilterKeyEventInternal(
      XK_A, ShiftMask | Mod1Mask, true));
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager->FilterKeyEventInternal(
      XK_A, ShiftMask | Mod1Mask, false));
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager->FilterKeyEventInternal(
      XK_Meta_L, ShiftMask | Mod1Mask, false));
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager->FilterKeyEventInternal(
      XK_Shift_L, ShiftMask, false));
}

// Press Shift, press Alt, press/release a, release Alt, press Alt again, then
// release Alt. In this case kNextEngineId event SHOULD be triggered, I believe.
TEST(HotkeyManagerTest, TestAltShiftAAlt) {
  scoped_ptr<TestableHotkeyManager> manager(CreateManager());
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager->FilterKeyEventInternal(
      XK_Shift_L, 0x0, true));
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager->FilterKeyEventInternal(
      XK_Meta_L, ShiftMask, true));
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager->FilterKeyEventInternal(
      XK_A, ShiftMask | Mod1Mask, true));
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager->FilterKeyEventInternal(
      XK_A, ShiftMask | Mod1Mask, false));
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager->FilterKeyEventInternal(
      XK_Meta_L, ShiftMask | Mod1Mask, false));
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager->FilterKeyEventInternal(
      XK_Meta_L, ShiftMask, true));
  EXPECT_EQ(kNextEngineId, manager->FilterKeyEventInternal(
      XK_Meta_L, ShiftMask | Mod1Mask, false));
  EXPECT_EQ(TestableHotkeyManager::kFiltered, manager->FilterKeyEventInternal(
      XK_Shift_L, ShiftMask, false));
}

// Press Shift, then press Alt, then press a. The release event for a is
// consumed by the window manager. In this case, kNextEngineId event should NOT
// be triggered.
TEST(HotkeyManagerTest, TestAltShiftANoKeyReleaseOfA) {
  scoped_ptr<TestableHotkeyManager> manager(CreateManager());
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager->FilterKeyEventInternal(
      XK_Shift_L, 0x0, true));
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager->FilterKeyEventInternal(
      XK_Meta_L, ShiftMask, true));
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager->FilterKeyEventInternal(
      XK_A, ShiftMask | Mod1Mask, true));
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager->FilterKeyEventInternal(
      XK_Meta_L, ShiftMask | Mod1Mask, false));
  EXPECT_EQ(TestableHotkeyManager::kNoEvent, manager->FilterKeyEventInternal(
      XK_Shift_L, ShiftMask, false));
}

TEST(HotkeyManagerTest, TestUnknownEvent) {
  TestableHotkeyManager manager;
  XEvent bad_event;
  bad_event.type = LASTEvent;
  EXPECT_FALSE(manager.FilterKeyEvent(bad_event));
}

}  // namespace input_method
}  // namespace chromeos
