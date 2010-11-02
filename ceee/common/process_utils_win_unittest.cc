// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for process utils for Windows.

#include "base/scoped_ptr.h"
#include "base/win/windows_version.h"
#include "ceee/common/process_utils_win.h"

#include "ceee/testing/utils/mock_win32.h"
#include "ceee/testing/utils/mock_static.h"
#include "ceee/testing/utils/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using process_utils_win::ProcessCompatibilityCheck;
using testing::_;
using testing::Invoke;
using testing::NativeSystemInfoMockTool;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrictMock;
using testing::WithArgs;

MOCK_STATIC_CLASS_BEGIN(MockProcessTokenAccess)
  MOCK_STATIC_INIT_BEGIN(MockProcessTokenAccess)
    MOCK_STATIC_INIT2(base::GetProcessIntegrityLevel, GetProcessIntegrityLevel);
    MOCK_STATIC_INIT(GetTokenInformation);
    MOCK_STATIC_INIT2(base::win::GetVersion, GetVersion);
  MOCK_STATIC_INIT_END()

  MOCK_STATIC2(bool, , GetProcessIntegrityLevel, HANDLE, base::IntegrityLevel*);
  MOCK_STATIC5(BOOL, WINAPI, GetTokenInformation, HANDLE,
               TOKEN_INFORMATION_CLASS, void*, DWORD, DWORD*);
  MOCK_STATIC0(base::win::Version, , GetVersion);
MOCK_STATIC_CLASS_END(MockProcessTokenAccess)

class TokenInfoExpectations {
 public:
  TokenInfoExpectations(BOOL return_value, TOKEN_ELEVATION_TYPE set_value)
      : return_value_(return_value), set_value_(set_value) {
  }

  BOOL QueryInformation(void* value_carrier) {
    TOKEN_ELEVATION_TYPE* value_carrier_coerce =
        reinterpret_cast<TOKEN_ELEVATION_TYPE*>(value_carrier);
    *value_carrier_coerce = set_value_;

    return return_value_;
  }

  BOOL return_value_;
  TOKEN_ELEVATION_TYPE set_value_;

 private:
  TokenInfoExpectations();
};


TEST(ProcessUtilsWin, SetThreadIntegrityLevel) {
  // TODO(mad@chromium.org): Test this!
}

TEST(ProcessUtilsWin, ResetThreadIntegrityLevel) {
  // TODO(mad@chromium.org): Test this too!
}


// This harness tests the implementation rather than the function and how
// well this method meets the requirement. Unfortunate, but necessary.
TEST(ProcessUtilsWin, IsRunningWithElevatedToken) {
  StrictMock<MockProcessTokenAccess> token_mock;
  bool running_admin = true;
  TokenInfoExpectations token_set(TRUE, TokenElevationTypeDefault);

  // In this function we check operation on systems with UAC
  EXPECT_CALL(token_mock, GetVersion())
      .WillRepeatedly(Return(base::win::VERSION_VISTA));

  EXPECT_CALL(token_mock, GetTokenInformation(_, TokenElevationType, _, _, _))
      .WillOnce(WithArgs<2>(
          Invoke(&token_set, &TokenInfoExpectations::QueryInformation)));

  ASSERT_HRESULT_SUCCEEDED(
      process_utils_win::IsCurrentProcessUacElevated(&running_admin));
  ASSERT_FALSE(running_admin);

  token_set.set_value_ = TokenElevationTypeLimited;
  EXPECT_CALL(token_mock, GetTokenInformation(_, TokenElevationType, _, _, _))
      .WillOnce(WithArgs<2>(
          Invoke(&token_set, &TokenInfoExpectations::QueryInformation)));
  ASSERT_HRESULT_SUCCEEDED(
      process_utils_win::IsCurrentProcessUacElevated(&running_admin));
  ASSERT_FALSE(running_admin);

  token_set.set_value_ = TokenElevationTypeFull;
  EXPECT_CALL(token_mock, GetTokenInformation(_, TokenElevationType, _, _, _))
      .WillOnce(WithArgs<2>(
          Invoke(&token_set, &TokenInfoExpectations::QueryInformation)));
  ASSERT_HRESULT_SUCCEEDED(
      process_utils_win::IsCurrentProcessUacElevated(&running_admin));
  ASSERT_TRUE(running_admin);
}

TEST(ProcessUtilsWin, IsRunningWithElevatedTokenOnXP) {
  StrictMock<MockProcessTokenAccess> token_mock;
  bool running_admin = true;

  EXPECT_CALL(token_mock, GetVersion())
      .WillOnce(Return(base::win::VERSION_XP));
  ASSERT_HRESULT_SUCCEEDED(
      process_utils_win::IsCurrentProcessUacElevated(&running_admin));
  ASSERT_FALSE(running_admin);  // No UAC on WinXP and thus no elevation.
}

TEST(ProcessUtilsWin, IsRunningWithElevatedTokenError) {
  StrictMock<MockProcessTokenAccess> token_mock;
  bool running_admin = true;
  TokenInfoExpectations token_set(FALSE, TokenElevationTypeDefault);

  EXPECT_CALL(token_mock, GetVersion())
      .WillOnce(Return(base::win::VERSION_WIN7));
  EXPECT_CALL(token_mock, GetTokenInformation(_, TokenElevationType, _, _, _))
          .WillOnce(WithArgs<2>(
              Invoke(&token_set, &TokenInfoExpectations::QueryInformation)));
  ASSERT_HRESULT_FAILED(
      process_utils_win::IsCurrentProcessUacElevated(&running_admin));
}

// Test harness' base for telling native 64 processes from WOW64 processes.
class ProcessCompatibilityCheckTest : public testing::Test {
 public:
  ProcessCompatibilityCheckTest() {
  }

 protected:
  // The class inherits from the utility under test to permit injecting test
  // code without risky patching of WinAPI.
  // This is not 'black box', since I need to presume a bit about the sequence
  // of calls.
  class TestingProcessCompatibility : public ProcessCompatibilityCheck {
   public:
    static void SetUpSingletonProcess(WORD system_type,
                                      bool process_is64,
                                      base::win::Version system_version,
                                      base::IntegrityLevel process_level) {
      AssertProcessDescriptionCorrectess(system_type, system_version,
                                         process_is64, process_level);

      testing_as_wow_ = !process_is64 &&
          system_type == PROCESSOR_ARCHITECTURE_AMD64;
      mocked_system_type_ = system_type;
      windows_version_ = system_version;
      PatchState(system_type, testing_as_wow_,
                 system_version > base::win::VERSION_XP, process_level,
                 ProcessCompatibilityCheckTest::SpoofedOpenProcess,
                 ProcessCompatibilityCheckTest::SpoofedCloseHandle,
                 ProcessCompatibilityCheckTest::SpoofIsWow64Process);
    }

    // Sets tested singleton as simple 32bit process on Win32.
    static void CreateAs_x86() {
      SetUpSingletonProcess(PROCESSOR_ARCHITECTURE_INTEL, false,
                            base::win::VERSION_XP, base::INTEGRITY_UNKNOWN);
    }

    // Sets tested singleton either as a simple 64bit process on x64 or as
    // 32bit process on WOW64.
    static void CreateAsFull_Wow64(bool current_is_32) {
      SetUpSingletonProcess(PROCESSOR_ARCHITECTURE_AMD64, !current_is_32,
                            base::win::VERSION_XP, base::INTEGRITY_UNKNOWN);
    }

    static void Reset() {
      ResetState();
    }
  };

  class TestingProcessCompatibilityInstantiation
      : public ProcessCompatibilityCheck {
   public:
    TestingProcessCompatibilityInstantiation()
        : ProcessCompatibilityCheck() {
    }
    // New instance of what would have been a singleton (to test constructor
    // code). Created as if on a 32bit platform.
    static TestingProcessCompatibilityInstantiation* CreateAs_x86() {
      return CreateInstance(PROCESSOR_ARCHITECTURE_INTEL, false,
                            base::win::VERSION_XP, base::INTEGRITY_UNKNOWN);
    }

    // Created as if on a 64bit platform.
    static TestingProcessCompatibilityInstantiation* CreateAsFull_Wow64(
        bool current_is_32) {
      return CreateInstance(PROCESSOR_ARCHITECTURE_AMD64, !current_is_32,
                            base::win::VERSION_XP, base::INTEGRITY_UNKNOWN);
    }

    // Creates test instance of ProcessCompatibilityCheck to represent a
    // 'current process' as defined by parameters.
    static TestingProcessCompatibilityInstantiation* CreateInstance(
        WORD system_type, bool process_is64,
        base::win::Version system_version,
        base::IntegrityLevel process_level) {
      PatchState(ProcessCompatibilityCheckTest::SpoofedOpenProcess,
                 ProcessCompatibilityCheckTest::SpoofedCloseHandle,
                 ProcessCompatibilityCheckTest::SpoofIsWow64Process);
      StrictMock<testing::MockKernel32> kernel_calls;
      StrictMock<MockProcessTokenAccess> token_mock;

      AssertProcessDescriptionCorrectess(system_type, system_version,
                                         process_is64, process_level);
      testing_as_wow_ = !process_is64 &&
          system_type == PROCESSOR_ARCHITECTURE_AMD64;
      mocked_system_type_ = system_type;
      windows_version_ = system_version;

      EXPECT_CALL(token_mock, GetVersion()).WillOnce(Return(system_version));
      if (system_type == PROCESSOR_ARCHITECTURE_INTEL) {
        EXPECT_CALL(kernel_calls, GetNativeSystemInfo(_))
            .WillOnce(Invoke(NativeSystemInfoMockTool::SetEnvironment_x86));
      } else {
        EXPECT_CALL(kernel_calls, GetNativeSystemInfo(_))
            .WillOnce(Invoke(NativeSystemInfoMockTool::SetEnvironment_x64));
      }

      // If mocked system supports integrity levels, expect a query.
      if (system_version > base::win::VERSION_XP) {
        EXPECT_CALL(token_mock, GetProcessIntegrityLevel(_, _)).
            WillOnce(DoAll(SetArgumentPointee<1>(process_level), Return(true)));
      }

      return new TestingProcessCompatibilityInstantiation();
    }

    ~TestingProcessCompatibilityInstantiation() {
      ResetState();
    }

    HRESULT IsProcessCompatible(DWORD process_id, bool* is_compatible) {
      return ProcessCompatibilityCheck::IsProcessCompatible(process_id,
                                                            is_compatible);
    }
  };

  struct MockProcessRepresentation {
    MockProcessRepresentation()
      : process_is_64bit(false),
        process_integrity_level(base::INTEGRITY_UNKNOWN),
        compatibility_check_expectation(false) {
    }

    MockProcessRepresentation(bool is_64, base::IntegrityLevel integrity,
                              bool expect_compatible)
      : process_is_64bit(is_64), process_integrity_level(integrity),
        compatibility_check_expectation(expect_compatible) {
    }

    bool process_is_64bit;
    base::IntegrityLevel process_integrity_level;
    bool compatibility_check_expectation;
  };

  virtual void SetUp() {
    mock_processes_.clear();
    back_to_pid_map_.clear();
    testing_as_wow_ = false;
    mocked_system_type_ = PROCESSOR_ARCHITECTURE_INTEL;
    windows_version_ = base::win::VERSION_XP;
    next_proc_handle_ = 1;
  }

  virtual void TearDown() {
    ASSERT_TRUE(back_to_pid_map_.empty());
    TestingProcessCompatibility::Reset();
  }

  // Own version of IsWow64Process. Parameter names and types to match
  // kernel32 function's signature.
  static BOOL WINAPI SpoofIsWow64Process(HANDLE hProcess, PBOOL Wow64Process) {
    if (::GetCurrentProcess() == hProcess) {
      *Wow64Process = testing_as_wow_;
      return TRUE;
    }

    HandleToIdMap::const_iterator it_pid =
        back_to_pid_map_.find(hProcess);

    if (it_pid != back_to_pid_map_.end()) {
      ProcessIdMap::const_iterator found_it =
          mock_processes_.find(it_pid->second);
      *Wow64Process = found_it != mock_processes_.end() &&
                                  !found_it->second.process_is_64bit;
      return TRUE;
    } else {
      LOG(ERROR) << "Failed to recognize test handle." << hProcess
          << " A broken test!";
      return FALSE;
    }
  }

  // Own version of OpenProcess. Creates my own fake handle and counts issued
  // handles. Parameter names and types to match.
  static HANDLE WINAPI SpoofedOpenProcess(DWORD dwDesiredAccess,
                                          BOOL bInheritHandle,
                                          DWORD dwProcessId) {
    DCHECK((dwDesiredAccess & PROCESS_QUERY_INFORMATION) ==
           PROCESS_QUERY_INFORMATION);
    HANDLE fake_handle = reinterpret_cast<HANDLE>(next_proc_handle_);
    back_to_pid_map_[fake_handle] = dwProcessId;
    next_proc_handle_++;

    return fake_handle;
  }

  // Own version of CloseHandle. Must match process, counts my handles.
  static BOOL WINAPI SpoofedCloseHandle(HANDLE hObject) {
    return back_to_pid_map_.erase(hObject) > 0;
  }

  // Own version of GetProcessIntegrityLevel from base:, which in turn
  // invokes queries on process tokens.
  static bool MockProcessIntegrityLevel(HANDLE process,
                                        base::IntegrityLevel* level) {
    HandleToIdMap::const_iterator it_pid = back_to_pid_map_.find(process);
    if (it_pid != back_to_pid_map_.end()) {
      ProcessIdMap::const_iterator it_process =
          mock_processes_.find(it_pid->second);
      if (it_process != mock_processes_.end()) {
        *level = it_process->second.process_integrity_level;
        return true;
      }
    }
    return false;
  }

  static void MakeMockProcess(DWORD pid, bool process_64bit,
                              base::IntegrityLevel integrity,
                              bool check_will_succeeed) {
    DCHECK(mock_processes_.find(pid) == mock_processes_.end());
    DCHECK(pid < kWindowCounterBase);
    AssertProcessDescriptionCorrectess(mocked_system_type_, windows_version_,
                                       process_64bit, integrity);
    mock_processes_[pid] = MockProcessRepresentation(process_64bit, integrity,
                                                     check_will_succeeed);
  }

  static void PerformCompatibilityCheck(
      TestingProcessCompatibilityInstantiation* checker) {
    StrictMock<MockProcessTokenAccess> mock_token;
    StrictMock<testing::MockUser32> mock_user32;

    if (windows_version_ > base::win::VERSION_XP) {
      EXPECT_CALL(mock_token, GetProcessIntegrityLevel(_, _)).
          WillRepeatedly(Invoke(
              ProcessCompatibilityCheckTest::MockProcessIntegrityLevel));
    }

    if (checker != NULL) {
      for (ProcessIdMap::const_iterator it = mock_processes_.begin();
           it != mock_processes_.end(); ++it) {
        bool process_compatible = false;
        ASSERT_HRESULT_SUCCEEDED(checker->IsProcessCompatible(it->first,
            &process_compatible));
        EXPECT_EQ(process_compatible,
                  it->second.compatibility_check_expectation);
      }
    } else {
      EXPECT_CALL(mock_user32, GetWindowThreadProcessId(_, _))
        .WillRepeatedly(Invoke(
            ProcessCompatibilityCheckTest::MockThreadProcessFromWindow));
      for (ProcessIdMap::const_iterator it = mock_processes_.begin();
           it != mock_processes_.end(); ++it) {
        bool window_compatible = false;
        bool process_compatible = false;

        ASSERT_HRESULT_SUCCEEDED(
            ProcessCompatibilityCheck::IsCompatible(it->first,
                                                    &process_compatible));
        ASSERT_HRESULT_SUCCEEDED(
            ProcessCompatibilityCheck::IsCompatible(
                reinterpret_cast<HWND>(it->first + kWindowCounterBase),
                    &window_compatible));
        EXPECT_EQ(window_compatible, process_compatible);
        EXPECT_EQ(process_compatible,
                  it->second.compatibility_check_expectation);
      }
    }
  }

  static DWORD MockThreadProcessFromWindow(HWND window, DWORD* process_id) {
    int process_from_window = reinterpret_cast<int>(window) -
        kWindowCounterBase;
    DCHECK(process_from_window >= 0 &&
           process_from_window < kWindowCounterBase);
    *process_id = static_cast<DWORD>(process_from_window);
    return 42;
  }

  typedef std::map<DWORD, MockProcessRepresentation> ProcessIdMap;
  typedef std::map<HANDLE, DWORD> HandleToIdMap;

  static ProcessIdMap mock_processes_;
  static HandleToIdMap back_to_pid_map_;
  static unsigned int next_proc_handle_;

  static bool testing_as_wow_;
  static WORD mocked_system_type_;
  static base::win::Version windows_version_;

  static const int kWindowCounterBase = 1000;
 private:
  // Asserts that variables defining a process are consistent with each other.
  // In other words: can process defined by two latter parameters exist on the
  // platform defined by two former.
  static void AssertProcessDescriptionCorrectess(
      WORD system_type, base::win::Version windows_version,
      bool current_process_is64, base::IntegrityLevel process_integrity) {
    ASSERT_TRUE(system_type == PROCESSOR_ARCHITECTURE_INTEL ||
                system_type == PROCESSOR_ARCHITECTURE_AMD64);
    ASSERT_TRUE(windows_version >= base::win::VERSION_XP);

    ASSERT_TRUE(system_type == PROCESSOR_ARCHITECTURE_AMD64 ||
                !current_process_is64);  // No 64 bit processes on win32.

    ASSERT_TRUE(windows_version > base::win::VERSION_XP ||
                process_integrity == base::INTEGRITY_UNKNOWN);
    ASSERT_TRUE(windows_version == base::win::VERSION_XP ||
                process_integrity >= base::LOW_INTEGRITY);
  }
};

ProcessCompatibilityCheckTest::ProcessIdMap
    ProcessCompatibilityCheckTest::mock_processes_;
ProcessCompatibilityCheckTest::HandleToIdMap
    ProcessCompatibilityCheckTest::back_to_pid_map_;

bool ProcessCompatibilityCheckTest::testing_as_wow_ = false;
unsigned int ProcessCompatibilityCheckTest::next_proc_handle_ = 0;
WORD ProcessCompatibilityCheckTest::mocked_system_type_ =
    PROCESSOR_ARCHITECTURE_INTEL;
base::win::Version ProcessCompatibilityCheckTest::windows_version_ =
    base::win::VERSION_XP;

TEST_F(ProcessCompatibilityCheckTest, AllIsGoodOnx86) {
  TestingProcessCompatibility::CreateAs_x86();

  bool all_is_good = false;
  ASSERT_HRESULT_SUCCEEDED(
      ProcessCompatibilityCheck::IsCompatible(42, &all_is_good));
  ASSERT_TRUE(all_is_good);
}

TEST_F(ProcessCompatibilityCheckTest, Only32PassSieveWithWow) {
  TestingProcessCompatibility::CreateAsFull_Wow64(true);

  const int wow_pool = 10;
  const int total_count = 15;

  for (int i = 1; i < total_count; ++i)
    MakeMockProcess(i, i >= wow_pool, base::INTEGRITY_UNKNOWN, i < wow_pool);
  PerformCompatibilityCheck(NULL);
}

TEST_F(ProcessCompatibilityCheckTest, Only64PassSieveWithNoWow) {
  TestingProcessCompatibility::CreateAsFull_Wow64(false);

  const int wow_pool = 10;
  const int total_count = 15;

  for (int i = 1; i < total_count; ++i)
    MakeMockProcess(i, i >= wow_pool, base::INTEGRITY_UNKNOWN, i >= wow_pool);
  PerformCompatibilityCheck(NULL);
}

TEST_F(ProcessCompatibilityCheckTest, AllIsGoodOnx86_WithInstantiation) {
  scoped_ptr<TestingProcessCompatibilityInstantiation>
      test_instance(TestingProcessCompatibilityInstantiation::CreateAs_x86());

  bool all_is_good = false;
  ASSERT_HRESULT_SUCCEEDED(
      test_instance->IsProcessCompatible(42, &all_is_good));
  ASSERT_TRUE(all_is_good);
}

TEST_F(ProcessCompatibilityCheckTest, Only32PassSieve_WithInstantiation) {
  scoped_ptr<TestingProcessCompatibilityInstantiation>
      test_instance(
          TestingProcessCompatibilityInstantiation::CreateAsFull_Wow64(true));
  const int wow_pool = 10;
  const int total_count = 15;

  for (int i = 1; i < total_count; ++i)
    MakeMockProcess(i, i >= wow_pool, base::INTEGRITY_UNKNOWN, i < wow_pool);
  PerformCompatibilityCheck(test_instance.get());
}

TEST_F(ProcessCompatibilityCheckTest, Only64PassSieve_WithInstantiation) {
  scoped_ptr<TestingProcessCompatibilityInstantiation>
      test_instance(
          TestingProcessCompatibilityInstantiation::CreateAsFull_Wow64(false));

  const int wow_pool = 10;
  const int total_count = 15;

  for (int i = 1; i < total_count; ++i)
    MakeMockProcess(i, i >= wow_pool, base::INTEGRITY_UNKNOWN, i >= wow_pool);
  PerformCompatibilityCheck(test_instance.get());
}

TEST_F(ProcessCompatibilityCheckTest, MediumIntegrityFilterOnWin32_Instance) {
  scoped_ptr<TestingProcessCompatibilityInstantiation>
      test_instance(
          TestingProcessCompatibilityInstantiation::CreateInstance(
              PROCESSOR_ARCHITECTURE_INTEL, false,
              base::win::VERSION_WIN7, base::MEDIUM_INTEGRITY));

  MakeMockProcess(1, false, base::MEDIUM_INTEGRITY, true);
  MakeMockProcess(2, false, base::LOW_INTEGRITY, true);
  MakeMockProcess(3, false, base::HIGH_INTEGRITY, false);  // Can't access.
  MakeMockProcess(4, false, base::MEDIUM_INTEGRITY, true);
  PerformCompatibilityCheck(test_instance.get());
}

TEST_F(ProcessCompatibilityCheckTest, HighIntegrityFilterOnWin32_Instance) {
  scoped_ptr<TestingProcessCompatibilityInstantiation>
      test_instance(
          TestingProcessCompatibilityInstantiation::CreateInstance(
              PROCESSOR_ARCHITECTURE_INTEL, false,
              base::win::VERSION_WIN7, base::HIGH_INTEGRITY));

  MakeMockProcess(1, false, base::MEDIUM_INTEGRITY, false);
  MakeMockProcess(2, false, base::LOW_INTEGRITY, false);
  MakeMockProcess(3, false, base::HIGH_INTEGRITY, true);
  MakeMockProcess(4, false, base::MEDIUM_INTEGRITY, false);
  PerformCompatibilityCheck(test_instance.get());
}

TEST_F(ProcessCompatibilityCheckTest, MediumIntegrityFilterOnWow64_Instance) {
  scoped_ptr<TestingProcessCompatibilityInstantiation>
      test_instance(
          TestingProcessCompatibilityInstantiation::CreateInstance(
              PROCESSOR_ARCHITECTURE_AMD64, false,
              base::win::VERSION_WIN7, base::MEDIUM_INTEGRITY));

  MakeMockProcess(1, false, base::MEDIUM_INTEGRITY, true);
  MakeMockProcess(2, false, base::LOW_INTEGRITY, true);
  MakeMockProcess(3, false, base::HIGH_INTEGRITY, false);
  MakeMockProcess(4, false, base::MEDIUM_INTEGRITY, true);
  MakeMockProcess(5, true, base::MEDIUM_INTEGRITY, false);
  MakeMockProcess(6, true, base::HIGH_INTEGRITY, false);
  PerformCompatibilityCheck(test_instance.get());
}

TEST_F(ProcessCompatibilityCheckTest, MediumIntegrityFilterOnWow64) {
  TestingProcessCompatibility::SetUpSingletonProcess(
      PROCESSOR_ARCHITECTURE_AMD64, false,
      base::win::VERSION_WIN7, base::MEDIUM_INTEGRITY);

  MakeMockProcess(1, false, base::MEDIUM_INTEGRITY, true);
  MakeMockProcess(2, false, base::LOW_INTEGRITY, true);
  MakeMockProcess(3, false, base::HIGH_INTEGRITY, false);
  MakeMockProcess(4, false, base::MEDIUM_INTEGRITY, true);
  MakeMockProcess(5, true, base::MEDIUM_INTEGRITY, false);
  MakeMockProcess(6, true, base::HIGH_INTEGRITY, false);
  PerformCompatibilityCheck(NULL);
}

}  // namespace
