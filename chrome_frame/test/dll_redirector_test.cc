// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/dll_redirector.h"

#include "base/shared_memory.h"
#include "base/utf_string_conversions.h"
#include "base/version.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "gtest/gtest.h"

extern "C" IMAGE_DOS_HEADER __ImageBase;

const char kMockVersionString[] = "42.42.42.42";
const char kMockVersionString2[] = "133.33.33.7";

const HMODULE kMockModuleHandle = reinterpret_cast<HMODULE>(42);
const HMODULE kMockModuleHandle2 = reinterpret_cast<HMODULE>(43);

const char kTestVersionBeaconName[] = "DllRedirectorTestVersionBeacon";
const uint32 kSharedMemorySize = 128;

// The maximum amount of time we are willing to let a test that Waits timeout
// before failing.
const uint32 kWaitTestTimeout = 20000;

class MockDllRedirector : public DllRedirector {
 public:
  explicit MockDllRedirector(const char* beacon_name)
      : DllRedirector(beacon_name) {}

  virtual HMODULE LoadVersionedModule() {
    return kMockModuleHandle;
  }

  virtual Version* GetCurrentModuleVersion() {
    return Version::GetVersionFromString(kMockVersionString);
  }

  virtual HMODULE GetFirstModule() {
    return DllRedirector::GetFirstModule();
  }

  Version* GetFirstModuleVersion() {
    // Lazy man's copy.
    return Version::GetVersionFromString(dll_version_->GetString());
  }

  base::SharedMemory* shared_memory() {
    return shared_memory_.get();
  }
};

class MockDllRedirector2 : public MockDllRedirector {
 public:
  explicit MockDllRedirector2(const char* beacon_name)
      : MockDllRedirector(beacon_name) {}

  virtual HMODULE LoadVersionedModule() {
    return kMockModuleHandle2;
  }

  virtual Version* GetCurrentModuleVersion() {
    return Version::GetVersionFromString(kMockVersionString2);
  }
};

class MockDllRedirectorNoPermissions : public MockDllRedirector {
 public:
  explicit MockDllRedirectorNoPermissions(const char* beacon_name)
    : MockDllRedirector(beacon_name) {}

  virtual bool BuildSecurityAttributesForLock(
      ATL::CSecurityAttributes* sec_attr) {
    return false;
  }

  virtual bool SetFileMappingToReadOnly(base::SharedMemoryHandle mapping) {
    return true;
  }
};

class DllRedirectorTest : public testing::Test {
 public:
  virtual void SetUp() {
    shared_memory_.reset(new base::SharedMemory);
    mock_version_.reset(Version::GetVersionFromString(kMockVersionString));
    mock_version2_.reset(Version::GetVersionFromString(kMockVersionString2));
  }

  virtual void TearDown() {
    CloseBeacon();
  }

  void CreateVersionBeacon(const std::string& name,
                           const std::string& version_string) {
    // Abort the test if we can't create and map a new named memory object.
    EXPECT_TRUE(shared_memory_->CreateNamed(name, false,
                                            kSharedMemorySize));
    EXPECT_TRUE(shared_memory_->Map(0));
    EXPECT_TRUE(shared_memory_->memory());

    if (shared_memory_->memory()) {
      memcpy(shared_memory_->memory(),
             version_string.c_str(),
             std::min(kSharedMemorySize, version_string.length() + 1));
    }
  }

  // Opens the named beacon and returns the version.
  Version* OpenAndReadVersionFromBeacon(const std::string& name) {
    // Abort the test if we can't open and map the named memory object.
    EXPECT_TRUE(shared_memory_->Open(name, true /* read_only */));
    EXPECT_TRUE(shared_memory_->Map(0));
    EXPECT_TRUE(shared_memory_->memory());

    char buffer[kSharedMemorySize] = {0};
    memcpy(buffer, shared_memory_->memory(), kSharedMemorySize - 1);
    return Version::GetVersionFromString(buffer);
  }

  void CloseBeacon() {
    shared_memory_->Close();
  }

  // Shared memory segment that contains the version beacon.
  scoped_ptr<base::SharedMemory> shared_memory_;
  scoped_ptr<Version> mock_version_;
  scoped_ptr<Version> mock_version2_;
};

TEST_F(DllRedirectorTest, RegisterAsFirstModule) {
  scoped_ptr<MockDllRedirector> redirector(
      new MockDllRedirector(kTestVersionBeaconName));
  EXPECT_TRUE(redirector->RegisterAsFirstCFModule());

  base::SharedMemory* redirector_memory = redirector->shared_memory();
  char buffer[kSharedMemorySize] = {0};
  memcpy(buffer, redirector_memory->memory(), kSharedMemorySize - 1);
  scoped_ptr<Version> redirector_version(Version::GetVersionFromString(buffer));
  ASSERT_TRUE(redirector_version.get());
  EXPECT_TRUE(redirector_version->Equals(*mock_version_.get()));
  redirector_memory = NULL;

  scoped_ptr<Version> memory_version(
      OpenAndReadVersionFromBeacon(kTestVersionBeaconName));
  ASSERT_TRUE(memory_version.get());
  EXPECT_TRUE(redirector_version->Equals(*memory_version.get()));
  CloseBeacon();

  redirector.reset();
  EXPECT_FALSE(shared_memory_->Open(kTestVersionBeaconName, true));
}

TEST_F(DllRedirectorTest, SecondModuleLoading) {
  scoped_ptr<MockDllRedirector> first_redirector(
      new MockDllRedirector(kTestVersionBeaconName));
  EXPECT_TRUE(first_redirector->RegisterAsFirstCFModule());

  scoped_ptr<MockDllRedirector2> second_redirector(
      new MockDllRedirector2(kTestVersionBeaconName));
  EXPECT_FALSE(second_redirector->RegisterAsFirstCFModule());

  scoped_ptr<Version> first_redirector_version(
      first_redirector->GetFirstModuleVersion());
  scoped_ptr<Version> second_redirector_version(
      second_redirector->GetFirstModuleVersion());

  EXPECT_TRUE(
      second_redirector_version->Equals(*first_redirector_version.get()));
  EXPECT_TRUE(
      second_redirector_version->Equals(*mock_version_.get()));
}

// This test ensures that the beacon remains alive as long as there is a single
// module that used it to determine its version still loaded.
TEST_F(DllRedirectorTest, TestBeaconOwnershipHandoff) {
  scoped_ptr<MockDllRedirector> first_redirector(
      new MockDllRedirector(kTestVersionBeaconName));
  EXPECT_TRUE(first_redirector->RegisterAsFirstCFModule());

  scoped_ptr<MockDllRedirector2> second_redirector(
      new MockDllRedirector2(kTestVersionBeaconName));
  EXPECT_FALSE(second_redirector->RegisterAsFirstCFModule());

  scoped_ptr<Version> first_redirector_version(
      first_redirector->GetFirstModuleVersion());
  scoped_ptr<Version> second_redirector_version(
      second_redirector->GetFirstModuleVersion());

  EXPECT_TRUE(
      second_redirector_version->Equals(*first_redirector_version.get()));
  EXPECT_TRUE(
      second_redirector_version->Equals(*mock_version_.get()));

  // Clear out the first redirector. The second, still holding a reference
  // to the shared memory should ensure that the beacon stays alive.
  first_redirector.reset();

  scoped_ptr<MockDllRedirector2> third_redirector(
      new MockDllRedirector2(kTestVersionBeaconName));
  EXPECT_FALSE(third_redirector->RegisterAsFirstCFModule());

  scoped_ptr<Version> third_redirector_version(
      third_redirector->GetFirstModuleVersion());

  EXPECT_TRUE(
      third_redirector_version->Equals(*second_redirector_version.get()));
  EXPECT_TRUE(
      third_redirector_version->Equals(*mock_version_.get()));

  // Now close all remaining redirectors, which should destroy the beacon.
  second_redirector.reset();
  third_redirector.reset();

  // Now create a fourth, expecting that this time it should be the first in.
  scoped_ptr<MockDllRedirector2> fourth_redirector(
      new MockDllRedirector2(kTestVersionBeaconName));
  EXPECT_TRUE(fourth_redirector->RegisterAsFirstCFModule());

  scoped_ptr<Version> fourth_redirector_version(
      fourth_redirector->GetFirstModuleVersion());

  EXPECT_TRUE(
      fourth_redirector_version->Equals(*mock_version2_.get()));
}

struct LockSquattingThreadParams {
  base::win::ScopedHandle is_squatting;
  base::win::ScopedHandle time_to_die;
};

DWORD WINAPI LockSquattingThread(void* in_params) {
  LockSquattingThreadParams* params =
      reinterpret_cast<LockSquattingThreadParams*>(in_params);
  DCHECK(params);

  // Grab the lock for the shared memory region and hold onto it.
  base::SharedMemory squatter(ASCIIToWide(kTestVersionBeaconName));
  base::SharedMemoryAutoLock squatter_lock(&squatter);

  // Notify our caller that we're squatting.
  BOOL ret = ::SetEvent(params->is_squatting);
  DCHECK(ret);

  // And then wait to be told to shut down.
  DWORD result = ::WaitForSingleObject(params->time_to_die, kWaitTestTimeout);
  EXPECT_EQ(WAIT_OBJECT_0, result);

  return 0;
}

// Test that the Right Thing happens when someone else is holding onto the
// beacon lock and not letting go. (The Right Thing being that the redirector
// assumes that it is the right version and doesn't attempt to use the shared
// memory region.)
TEST_F(DllRedirectorTest, LockSquatting) {
  scoped_ptr<MockDllRedirector> first_redirector(
      new MockDllRedirector(kTestVersionBeaconName));
  EXPECT_TRUE(first_redirector->RegisterAsFirstCFModule());

  LockSquattingThreadParams params;
  params.is_squatting.Set(::CreateEvent(NULL, FALSE, FALSE, NULL));
  params.time_to_die.Set(::CreateEvent(NULL, FALSE, FALSE, NULL));
  DWORD tid = 0;
  base::win::ScopedHandle lock_squat_thread(
      ::CreateThread(NULL, 0, LockSquattingThread, &params, 0, &tid));

  // Make sure the squatter has started squatting.
  DWORD wait_result = ::WaitForSingleObject(params.is_squatting,
                                            kWaitTestTimeout);
  EXPECT_EQ(WAIT_OBJECT_0, wait_result);

  scoped_ptr<MockDllRedirector2> second_redirector(
      new MockDllRedirector2(kTestVersionBeaconName));
  EXPECT_TRUE(second_redirector->RegisterAsFirstCFModule());

  scoped_ptr<Version> second_redirector_version(
      second_redirector->GetFirstModuleVersion());
  EXPECT_TRUE(
      second_redirector_version->Equals(*mock_version2_.get()));

  // Shut down the squatting thread.
  DWORD ret = ::SetEvent(params.time_to_die);
  DCHECK(ret);

  wait_result = ::WaitForSingleObject(lock_squat_thread, kWaitTestTimeout);
  EXPECT_EQ(WAIT_OBJECT_0, wait_result);
}

TEST_F(DllRedirectorTest, BadVersionNumber) {
  std::string bad_version("I am not a version number");
  CreateVersionBeacon(kTestVersionBeaconName, bad_version);

  // The redirector should fail to read the version number and defer to
  // its own version.
  scoped_ptr<MockDllRedirector> first_redirector(
      new MockDllRedirector(kTestVersionBeaconName));
  EXPECT_TRUE(first_redirector->RegisterAsFirstCFModule());

  HMODULE first_module = first_redirector->GetFirstModule();
  EXPECT_EQ(NULL, first_module);
}

// TODO(robertshield): These tests rely on simulating access checks from a low
// integrity process using impersonation. This may not be exactly identical to
// actually having a separate low integrity process.
TEST_F(DllRedirectorTest, LowIntegrityAccess) {
  scoped_ptr<MockDllRedirector> first_redirector(
      new MockDllRedirector(kTestVersionBeaconName));
  EXPECT_TRUE(first_redirector->RegisterAsFirstCFModule());

  // Ensure that we can acquire the mutex from medium integrity:
  {
    base::SharedMemory shared_memory(ASCIIToWide(kTestVersionBeaconName));
    bool mutex_locked = shared_memory.Lock(kWaitTestTimeout, NULL);
    EXPECT_TRUE(mutex_locked);

    // Ensure that the shared memory is read-only:
    EXPECT_FALSE(shared_memory.Open(kTestVersionBeaconName, false));
    shared_memory.Close();
    EXPECT_TRUE(shared_memory.Open(kTestVersionBeaconName, true));
    shared_memory.Close();

    if (mutex_locked)
      shared_memory.Unlock();
  }

  if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
    // Now move to low integrity
    chrome_frame_test::LowIntegrityToken low_integrity_token;
    ASSERT_TRUE(low_integrity_token.Impersonate());

    // Ensure that we can also acquire the mutex from low integrity.
    base::SharedMemory shared_memory(ASCIIToWide(kTestVersionBeaconName));
    bool mutex_locked = shared_memory.Lock(kWaitTestTimeout, NULL);
    EXPECT_TRUE(mutex_locked);

    // Ensure that the shared memory is read-only:
    EXPECT_FALSE(shared_memory.Open(kTestVersionBeaconName, false));
    shared_memory.Close();
    EXPECT_TRUE(shared_memory.Open(kTestVersionBeaconName, true));
    shared_memory.Close();

    if (mutex_locked)
      shared_memory.Unlock();
  }
}

TEST_F(DllRedirectorTest, LowIntegrityAccessDenied) {
  // Run this test with a mock DllRedirector that doesn't set permissions
  // on the shared memory.
  scoped_ptr<MockDllRedirectorNoPermissions> first_redirector(
      new MockDllRedirectorNoPermissions(kTestVersionBeaconName));
  EXPECT_TRUE(first_redirector->RegisterAsFirstCFModule());

  // Ensure that we can acquire the mutex from medium integrity:
  {
    base::SharedMemory shared_memory(ASCIIToWide(kTestVersionBeaconName));
    bool mutex_locked = shared_memory.Lock(kWaitTestTimeout, NULL);
    EXPECT_TRUE(mutex_locked);

    // We should be able to open the memory as read/write.
    EXPECT_TRUE(shared_memory.Open(kTestVersionBeaconName, false));
    shared_memory.Close();

    if (mutex_locked)
      shared_memory.Unlock();
  }

  if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
    // Now move to low integrity
    chrome_frame_test::LowIntegrityToken low_integrity_token;
    low_integrity_token.Impersonate();

    // Ensure that we can't acquire the mutex without having set the
    // Low Integrity ACE in the SACL.
    base::SharedMemory shared_memory(ASCIIToWide(kTestVersionBeaconName));
    bool mutex_locked = shared_memory.Lock(kWaitTestTimeout, NULL);
    EXPECT_FALSE(mutex_locked);

    // We shouldn't be able to open the memory.
    EXPECT_FALSE(shared_memory.Open(kTestVersionBeaconName, false));
    shared_memory.Close();

    if (mutex_locked)
      shared_memory.Unlock();
  }
}
