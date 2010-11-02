// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Tests for utilities for windows (as in HWND).

#include "ceee/common/window_utils.h"

#include "base/process_util.h"
#include "ceee/common/process_utils_win.h"
#include "ceee/testing/utils/mock_win32.h"
#include "ceee/testing/utils/test_utils.h"
#include "ceee/common/windows_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::NotNull;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrictMock;
using testing::WithArgs;

TEST(WindowUtils, IsWindowClas) {
  HWND window = ::GetDesktopWindow();
  ASSERT_TRUE(window);
  EXPECT_TRUE(window_utils::IsWindowClass(window,
                                          windows::kDesktopWindowClass));
  EXPECT_FALSE(window_utils::IsWindowClass(
      window, std::wstring(windows::kDesktopWindowClass) + L"stop"));
}

TEST(WindowUtils, IsWindowClassNotWindow) {
  testing::LogDisabler no_dcheck;
  ASSERT_FALSE(window_utils::IsWindowClass(NULL, L"Foofoo"));
}

TEST(WindowUtils, GetTopLevelParent) {
  // Identity
  HWND desktop_window = ::GetDesktopWindow();
  ASSERT_EQ(desktop_window, window_utils::GetTopLevelParent(desktop_window));

  // Child of a child
  HWND tray_window = ::FindWindowEx(NULL, NULL,
                                    windows::kShellTrayWindowClass, NULL);
  HWND notify_window = ::FindWindowEx(tray_window, NULL,
                                      windows::kTrayNotifyWindowClass, NULL);
  HWND child_of_notify_window = ::FindWindowEx(notify_window, NULL,
                                               NULL, NULL);
  ASSERT_TRUE(tray_window);
  ASSERT_TRUE(child_of_notify_window);
  ASSERT_EQ(tray_window,
            window_utils::GetTopLevelParent(child_of_notify_window));

  // Not a window
  StrictMock<testing::MockUser32> user32;
  EXPECT_CALL(user32, GetParent(_)).WillOnce(Return((HWND)NULL));
  ASSERT_EQ((HWND)0x42, window_utils::GetTopLevelParent((HWND)0x42));
}

TEST(WindowUtils, IsWindowThread) {
  DWORD process_id = ::GetCurrentProcessId();
  DWORD thread_id = ::GetCurrentThreadId();

  // Success.
  StrictMock<testing::MockUser32> user32;
  EXPECT_CALL(user32, GetWindowThreadProcessId((HWND)42, NotNull())).
      WillOnce(DoAll(SetArgumentPointee<1>(process_id), Return(thread_id)));
  EXPECT_TRUE(window_utils::IsWindowThread((HWND)42));

  // Wrong thread.
  EXPECT_CALL(user32, GetWindowThreadProcessId((HWND)42, NotNull())).
      WillOnce(DoAll(SetArgumentPointee<1>(process_id), Return(0)));
  EXPECT_FALSE(window_utils::IsWindowThread((HWND)42));

  // Wrong process.
  EXPECT_CALL(user32, GetWindowThreadProcessId((HWND)42, NotNull())).
      WillOnce(DoAll(SetArgumentPointee<1>(0), Return(thread_id)));
  EXPECT_FALSE(window_utils::IsWindowThread((HWND)42));
}

TEST(WindowUtils, WindowHasNoThread) {
  // Success.
  StrictMock<testing::MockUser32> user32;
  EXPECT_CALL(user32, GetWindowThreadProcessId((HWND)42, NULL)).
      WillOnce(Return(1));  // Any non-zero value will do...
  EXPECT_FALSE(window_utils::WindowHasNoThread((HWND)42));

  // No thread for window.
  EXPECT_CALL(user32, GetWindowThreadProcessId((HWND)42, NULL)).
    WillOnce(Return(0));
  EXPECT_TRUE(window_utils::WindowHasNoThread((HWND)42));
}

MOCK_STATIC_CLASS_BEGIN(MockProcessIntegrityQuery)
  MOCK_STATIC_INIT_BEGIN(MockProcessIntegrityQuery)
    MOCK_STATIC_INIT2(base::GetProcessIntegrityLevel,
                      GetProcessIntegrityLevel);
  MOCK_STATIC_INIT_END()

  MOCK_STATIC2(bool, , GetProcessIntegrityLevel, HANDLE,
               base::IntegrityLevel*);
MOCK_STATIC_CLASS_END(MockProcessIntegrityQuery)

MOCK_STATIC_CLASS_BEGIN(PartialMockWindowUtils)
  MOCK_STATIC_INIT_BEGIN(PartialMockWindowUtils)
    MOCK_STATIC_INIT2(window_utils::IsWindowClass, IsWindowClass);
  MOCK_STATIC_INIT_END()

  MOCK_STATIC2(bool, , IsWindowClass, HWND, const std::wstring&);
MOCK_STATIC_CLASS_END(PartialMockWindowUtils)


class MockCompatibilityCheck {
 public:
  MockCompatibilityCheck(bool on_64_bit,
                         base::IntegrityLevel current_process_integrity) {
    MockAccess::SetUp(on_64_bit,
                      current_process_integrity != base::INTEGRITY_UNKNOWN,
                      current_process_integrity);
    mock_integrity_ = new MockProcessIntegrityQuery();
    EXPECT_CALL(*mock_integrity_, GetProcessIntegrityLevel(_, _)).
        Times(testing::AnyNumber()).WillRepeatedly(Invoke(
            MockCompatibilityCheck::MockProcessIntegrityLevel));
    EXPECT_CALL(mock_user32_, GetWindowThreadProcessId(_, _)).
        Times(testing::AnyNumber()).WillRepeatedly(Invoke(
            MockCompatibilityCheck::MockThreadProcessFromWindow));
    EXPECT_CALL(mock_user32_, IsWindow(_)).Times(0);
    EXPECT_CALL(mock_user32_, IsWindowVisible(_)).
        Times(testing::AnyNumber()).WillRepeatedly(Invoke(
            MockCompatibilityCheck::MockIsWindowVisible));
    EXPECT_CALL(mock_window_tests_, IsWindowClass(_, _)).
        Times(testing::AnyNumber()).WillRepeatedly(Return(true));
  }

  ~MockCompatibilityCheck() {
    delete mock_integrity_;
    mock_integrity_ = NULL;
    for (WindowsMap::iterator it = mock_windows_.begin();
         it != mock_windows_.end(); ++it) {
      delete it->second;
    }
    mock_windows_.clear();
    process_id_reference_.clear();
    process_handle_reference_.clear();
    MockAccess::TearDown();
  }

  void AddWindow(HWND fake_window, bool visible,
                 base::IntegrityLevel owner_integrity, bool process_is_64) {
    ASSERT_TRUE(mock_windows_.find(fake_window) == mock_windows_.end());
    MockWindowRepresentation * new_window =
        new MockWindowRepresentation(fake_window, visible, process_is_64,
                                     owner_integrity);
    mock_windows_[fake_window] = new_window;
    process_id_reference_[new_window->owning_process_id] = new_window;
  }

  static BOOL MockEnumWindows(WNDENUMPROC enum_proc, LPARAM param) {
    for (WindowsMap::const_iterator it = mock_windows_.begin();
         it != mock_windows_.end(); ++it) {
      if (!(enum_proc(it->first, param)))
        break;
    }
    return TRUE;
  }

  StrictMock<testing::MockUser32> mock_user32_;

 protected:
  class MockAccess : public process_utils_win::ProcessCompatibilityCheck {
   public:
    static void SetUp(bool on_64_bit, bool integrity_checks,
                      base::IntegrityLevel current_process_integrity) {
      PatchState(on_64_bit ? PROCESSOR_ARCHITECTURE_AMD64
                           : PROCESSOR_ARCHITECTURE_INTEL,
                 on_64_bit, integrity_checks, current_process_integrity,
                 MockCompatibilityCheck::SpoofOpenProcess,
                 MockCompatibilityCheck::SpoofCloseHandle,
                 MockCompatibilityCheck::SpoofIsWow64Process);
    }

    static void TearDown() {
      ResetState();
    }
  };

  struct MockWindowRepresentation {
    MockWindowRepresentation(HWND window_handle, bool window_visible,
                             bool is_64_bit, base::IntegrityLevel integrity) {
      window = window_handle;
      visible = window_visible;
      owning_process_is_64 = is_64_bit;
      owning_process_integrity = integrity;
      owning_process_id = reinterpret_cast<int>(window_handle) + 1000;
    }

    HWND window;
    bool visible;
    DWORD owning_process_id;
    bool owning_process_is_64;
    base::IntegrityLevel owning_process_integrity;
  };

  // Own version of IsWow64Process. Parameter names and types to match
  // kernel32 function's signature.
  static BOOL WINAPI SpoofIsWow64Process(HANDLE hProcess, PBOOL Wow64Process) {
    ProcessHandleMap::const_iterator window_it =
        process_handle_reference_.find(hProcess);
    if (window_it != process_handle_reference_.end()) {
      *Wow64Process = !window_it->second->owning_process_is_64;
    } else {
      *Wow64Process = TRUE;
    }
    return TRUE;
  }

  // Own version of OpenProcess. Issues handles as required.
  static HANDLE WINAPI SpoofOpenProcess(DWORD dwDesiredAccess,
                                        BOOL bInheritHandle,
                                        DWORD dwProcessId) {
    ProcessIdMap::const_iterator found_window_it =
        process_id_reference_.find(dwProcessId);
    if (found_window_it != process_id_reference_.end()) {
      HANDLE expected_handle_value =
          reinterpret_cast<HANDLE>(found_window_it->second->window);
      process_handle_reference_[expected_handle_value] =
          found_window_it->second;
      return expected_handle_value;
    }

    return NULL;
  }

  static BOOL WINAPI SpoofCloseHandle(HANDLE hObject) {
    return process_handle_reference_.erase(hObject) == 1;
  }

  static bool MockProcessIntegrityLevel(HANDLE process,
                                        base::IntegrityLevel* level) {
    ProcessHandleMap::const_iterator window_it =
        process_handle_reference_.find(process);
    if (window_it != process_handle_reference_.end()) {
      *level = window_it->second->owning_process_integrity;
      return true;
    }
    return false;
  }

  static DWORD MockThreadProcessFromWindow(HWND window, DWORD* process_id) {
    WindowsMap::const_iterator window_it = mock_windows_.find(window);
    if (window_it != mock_windows_.end()) {
      *process_id = window_it->second->owning_process_id;
      return 42;
    } else {
      *process_id = 0;
      return 0;
    }
  }

  static BOOL MockIsWindowVisible(HWND window) {
    WindowsMap::const_iterator window_it = mock_windows_.find(window);
    if (window_it != mock_windows_.end())
      return window_it->second->visible;

    return FALSE;
  }

  typedef std::map<HWND, MockWindowRepresentation*> WindowsMap;
  typedef std::map<DWORD, MockWindowRepresentation*> ProcessIdMap;
  typedef std::map<HANDLE, MockWindowRepresentation*> ProcessHandleMap;

  MockProcessIntegrityQuery* mock_integrity_;
  PartialMockWindowUtils mock_window_tests_;

  static WindowsMap mock_windows_;
  static ProcessIdMap process_id_reference_;
  static ProcessHandleMap process_handle_reference_;
};

MockCompatibilityCheck::WindowsMap MockCompatibilityCheck::mock_windows_;
MockCompatibilityCheck::ProcessIdMap
    MockCompatibilityCheck::process_id_reference_;
MockCompatibilityCheck::ProcessHandleMap
    MockCompatibilityCheck::process_handle_reference_;

const HWND kDescendent = reinterpret_cast<HWND>(24);
struct FindDescendentData {
  const std::wstring& window_class;
  bool only_visible;
  HWND descendent;
};

BOOL MockEnumChildWindows(HWND ancestor, WNDENUMPROC enum_proc, LPARAM param) {
  FindDescendentData* data = reinterpret_cast<FindDescendentData*>(param);
  if (!data->only_visible)
    data->descendent = kDescendent;
  return TRUE;
}

TEST(WindowUtils, FindDescendentWindow) {
  testing::LogDisabler no_dcheck;

  // Test failure cases.
  StrictMock<testing::MockUser32> user32;
  const HWND kAncestor = reinterpret_cast<HWND>(42);
  HWND descendent = NULL;
  EXPECT_CALL(user32, EnumChildWindows(kAncestor, _, _)).
      WillOnce(Return(FALSE));
  EXPECT_FALSE(window_utils::FindDescendentWindow(kAncestor, L"",
                                                  false, &descendent));
  EXPECT_EQ(NULL, descendent);

  // Success.
  EXPECT_CALL(user32, EnumChildWindows(kAncestor, _, _)).
      WillRepeatedly(Invoke(MockEnumChildWindows));
  EXPECT_TRUE(window_utils::FindDescendentWindow(kAncestor, L"",
                                                 false, &descendent));
  EXPECT_EQ(kDescendent, descendent);

  // Don't find invisible window if we don't want them.
  descendent = NULL;
  EXPECT_FALSE(window_utils::FindDescendentWindow(kAncestor, L"",
                                                  true, &descendent));
  EXPECT_EQ(NULL, descendent);

  // Still work even if we are not interested in the window handle.
  EXPECT_TRUE(window_utils::FindDescendentWindow(kAncestor, L"", false, NULL));
}

TEST(WindowUtils, FindDescendentWindowWithFilter) {
  const HWND kAncestor = reinterpret_cast<HWND>(444);
  HWND descendent = NULL;
  MockCompatibilityCheck compatibility_check(true, base::MEDIUM_INTEGRITY);
  compatibility_check.AddWindow(reinterpret_cast<HWND>(38), true,
                                base::HIGH_INTEGRITY, false);  // Unreachable.
  compatibility_check.AddWindow(reinterpret_cast<HWND>(39), true,
                                base::LOW_INTEGRITY, true);   // Unreachable.
  compatibility_check.AddWindow(reinterpret_cast<HWND>(42), true,
                                base::MEDIUM_INTEGRITY, false);  // OK

  EXPECT_CALL(compatibility_check.mock_user32_,
              EnumChildWindows(kAncestor, _, _)).WillRepeatedly(
      WithArgs<1, 2>(Invoke(MockCompatibilityCheck::MockEnumWindows)));

  EXPECT_TRUE(window_utils::FindDescendentWindow(kAncestor, L"",
                                                 false, &descendent));
  EXPECT_EQ(reinterpret_cast<HWND>(42), descendent);
}

TEST(WindowUtils, FindVisibleDescendentWindowWithFilter) {
  const HWND kAncestor = reinterpret_cast<HWND>(444);
  HWND descendent = NULL;
  MockCompatibilityCheck compatibility_check(true, base::MEDIUM_INTEGRITY);
  compatibility_check.AddWindow(reinterpret_cast<HWND>(38), false,
                                base::HIGH_INTEGRITY, false);  // Unreachable.
  compatibility_check.AddWindow(reinterpret_cast<HWND>(39), true,
                                base::LOW_INTEGRITY, true);  // Unreachable.
  compatibility_check.AddWindow(reinterpret_cast<HWND>(40), false,
                                base::MEDIUM_INTEGRITY, false);  // Invisible.
  compatibility_check.AddWindow(reinterpret_cast<HWND>(42), true,
                                base::MEDIUM_INTEGRITY, false);  // OK

  EXPECT_CALL(compatibility_check.mock_user32_,
              EnumChildWindows(kAncestor, _, _)).WillRepeatedly(
      WithArgs<1, 2>(Invoke(MockCompatibilityCheck::MockEnumWindows)));

  EXPECT_TRUE(window_utils::FindDescendentWindow(kAncestor, L"",
                                                 true, &descendent));
  EXPECT_EQ(reinterpret_cast<HWND>(42), descendent);
}

TEST(WindowUtils, FindWindowsWithoutFilter) {
  std::set<HWND> out_set;
  MockCompatibilityCheck compatibility_check(true, base::MEDIUM_INTEGRITY);
  compatibility_check.AddWindow(reinterpret_cast<HWND>(38), true,
                                base::HIGH_INTEGRITY, false);  // Unreachable.
  compatibility_check.AddWindow(reinterpret_cast<HWND>(39), true,
                                base::LOW_INTEGRITY, true);  // Unreachable.
  compatibility_check.AddWindow(reinterpret_cast<HWND>(42), true,
                                base::MEDIUM_INTEGRITY, false);  // OK

  EXPECT_CALL(compatibility_check.mock_user32_,
              EnumWindows(_, _)).WillRepeatedly(Invoke(
      MockCompatibilityCheck::MockEnumWindows));

  window_utils::FindTopLevelWindowsEx(L"", false, &out_set);
  EXPECT_EQ(out_set.size(), 3);
  ASSERT_TRUE(out_set.find(reinterpret_cast<HWND>(38)) != out_set.end());
  ASSERT_TRUE(out_set.find(reinterpret_cast<HWND>(39)) != out_set.end());
  ASSERT_TRUE(out_set.find(reinterpret_cast<HWND>(42)) != out_set.end());
}

TEST(WindowUtils, FindWindowsWithFilter) {
  std::set<HWND> out_set;
  MockCompatibilityCheck compatibility_check(true, base::MEDIUM_INTEGRITY);
  compatibility_check.AddWindow(reinterpret_cast<HWND>(38), true,
                                base::HIGH_INTEGRITY, false);  // Unreachable.
  compatibility_check.AddWindow(reinterpret_cast<HWND>(39), true,
                                base::LOW_INTEGRITY, true);  // Unreachable.
  compatibility_check.AddWindow(reinterpret_cast<HWND>(42), true,
                                base::MEDIUM_INTEGRITY, false);  // OK

  EXPECT_CALL(compatibility_check.mock_user32_,
              EnumWindows(_, _)).WillRepeatedly(Invoke(
      MockCompatibilityCheck::MockEnumWindows));

  window_utils::FindTopLevelWindows(L"", &out_set);
  EXPECT_EQ(out_set.size(), 1);
  ASSERT_TRUE(out_set.find(reinterpret_cast<HWND>(42)) != out_set.end());
}


}  // namespace
