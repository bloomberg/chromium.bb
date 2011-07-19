// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/vtable_patch_manager.h"

#include <unknwn.h>

#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "base/win/scoped_handle.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace {
// GMock names we use.
using testing::_;
using testing::Return;

class MockClassFactory : public IClassFactory {
 public:
  MOCK_METHOD2_WITH_CALLTYPE(__stdcall, QueryInterface,
      HRESULT(REFIID riid, void **object));
  MOCK_METHOD0_WITH_CALLTYPE(__stdcall, AddRef, ULONG());
  MOCK_METHOD0_WITH_CALLTYPE(__stdcall, Release, ULONG());
  MOCK_METHOD3_WITH_CALLTYPE(__stdcall, CreateInstance,
        HRESULT (IUnknown *outer, REFIID riid, void **object));
  MOCK_METHOD1_WITH_CALLTYPE(__stdcall, LockServer, HRESULT(BOOL lock));
};

// Retrieve the vtable for an interface.
void* GetVtable(IUnknown* unk) {
  return *reinterpret_cast<void**>(unk);
}

// Forward decl.
extern vtable_patch::MethodPatchInfo IClassFactory_PatchInfo[];

class VtablePatchManagerTest: public testing::Test {
 public:
  VtablePatchManagerTest() {
    EXPECT_TRUE(current_ == NULL);
    current_ = this;
  }

  ~VtablePatchManagerTest() {
    EXPECT_TRUE(current_ == this);
    current_ = NULL;
  }

  virtual void SetUp() {
    // Make a backup of the test vtable and it's page protection settings.
    void* vtable = GetVtable(&factory_);
    MEMORY_BASIC_INFORMATION info;
    ASSERT_TRUE(::VirtualQuery(vtable, &info, sizeof(info)));
    vtable_protection_ = info.Protect;
    memcpy(vtable_backup_, vtable, sizeof(vtable_backup_));
  }

  virtual void TearDown() {
    // Unpatch to make sure we've restored state for subsequent test.
    UnpatchInterfaceMethods(IClassFactory_PatchInfo);

    // Restore the test vtable and its page protection settings.
    void* vtable = GetVtable(&factory_);
    DWORD old_protect = 0;
    EXPECT_TRUE(::VirtualProtect(vtable, sizeof(vtable_backup_),
        PAGE_EXECUTE_WRITECOPY, &old_protect));
    memcpy(vtable, vtable_backup_, sizeof(vtable_backup_));
    EXPECT_TRUE(::VirtualProtect(vtable, sizeof(vtable_backup_),
        vtable_protection_, &old_protect));
  }

  typedef HRESULT (__stdcall* LockServerFun)(IClassFactory* self, BOOL lock);
  MOCK_METHOD3(LockServerPatch,
      HRESULT(LockServerFun old_fun, IClassFactory* self, BOOL lock));

  static HRESULT STDMETHODCALLTYPE LockServerPatchCallback(
      LockServerFun fun, IClassFactory* self, BOOL lock) {
    EXPECT_TRUE(current_ != NULL);
    if (current_ != NULL)
      return current_->LockServerPatch(fun, self, lock);
    else
      return E_UNEXPECTED;
  }

 protected:
  // Number of functions in the IClassFactory vtable.
  static const size_t kFunctionCount = 5;

  // Backup of the factory_ vtable as we found it at Setup.
  PROC vtable_backup_[kFunctionCount];
  // VirtualProtect flags on the factory_ vtable as we found it at Setup.
  DWORD vtable_protection_;

  // The mock factory class we patch.
  MockClassFactory factory_;

  // Current test running for routing the patch callback function.
  static VtablePatchManagerTest* current_;
};

VtablePatchManagerTest* VtablePatchManagerTest::current_ = NULL;

BEGIN_VTABLE_PATCHES(IClassFactory)
  VTABLE_PATCH_ENTRY(4, &VtablePatchManagerTest::LockServerPatchCallback)
END_VTABLE_PATCHES();

}  // namespace

TEST_F(VtablePatchManagerTest, ReplacePointer) {
  void* const kFunctionOriginal = reinterpret_cast<void*>(0xCAFEBABE);
  void* const kFunctionFoo = reinterpret_cast<void*>(0xF0F0F0F0);
  void* const kFunctionBar = reinterpret_cast<void*>(0xBABABABA);

  using vtable_patch::internal::ReplaceFunctionPointer;
  // Replacing a non-writable location should fail, but not crash.
  EXPECT_FALSE(ReplaceFunctionPointer(NULL, kFunctionBar, kFunctionFoo));

  void* foo_entry = kFunctionOriginal;
  // Replacing with the wrong original function should
  // fail and not change the entry.
  EXPECT_FALSE(ReplaceFunctionPointer(&foo_entry, kFunctionBar, kFunctionFoo));
  EXPECT_EQ(foo_entry, kFunctionOriginal);

  // Replacing with the correct original should succeed.
  EXPECT_TRUE(ReplaceFunctionPointer(&foo_entry,
                                     kFunctionBar,
                                     kFunctionOriginal));
  EXPECT_EQ(foo_entry, kFunctionBar);
}

TEST_F(VtablePatchManagerTest, PatchInterfaceMethods) {
  // Unpatched.
  EXPECT_CALL(factory_, LockServer(TRUE))
      .WillOnce(Return(E_FAIL));
  EXPECT_EQ(E_FAIL, factory_.LockServer(TRUE));

  EXPECT_HRESULT_SUCCEEDED(
      PatchInterfaceMethods(&factory_, IClassFactory_PatchInfo));

  EXPECT_NE(0, memcmp(GetVtable(&factory_),
                      vtable_backup_,
                      sizeof(vtable_backup_)));

  // This should not be called while the patch is in effect.
  EXPECT_CALL(factory_, LockServer(_))
      .Times(0);

  EXPECT_CALL(*this, LockServerPatch(testing::_, &factory_, TRUE))
      .WillOnce(testing::Return(S_FALSE));

  EXPECT_EQ(S_FALSE, factory_.LockServer(TRUE));
}

TEST_F(VtablePatchManagerTest, UnpatchInterfaceMethods) {
  // Patch it.
  EXPECT_HRESULT_SUCCEEDED(
      PatchInterfaceMethods(&factory_, IClassFactory_PatchInfo));

  EXPECT_NE(0, memcmp(GetVtable(&factory_),
                      vtable_backup_,
                      sizeof(vtable_backup_)));

  // This should not be called while the patch is in effect.
  EXPECT_CALL(factory_, LockServer(testing::_))
      .Times(0);

  EXPECT_CALL(*this, LockServerPatch(testing::_, &factory_, TRUE))
      .WillOnce(testing::Return(S_FALSE));

  EXPECT_EQ(S_FALSE, factory_.LockServer(TRUE));

  // Now unpatch.
  EXPECT_HRESULT_SUCCEEDED(
      UnpatchInterfaceMethods(IClassFactory_PatchInfo));

  // And check that the call comes through correctly.
  EXPECT_CALL(factory_, LockServer(FALSE))
      .WillOnce(testing::Return(E_FAIL));
  EXPECT_EQ(E_FAIL, factory_.LockServer(FALSE));
}

TEST_F(VtablePatchManagerTest, DoublePatch) {
  // Patch it.
  EXPECT_HRESULT_SUCCEEDED(
      PatchInterfaceMethods(&factory_, IClassFactory_PatchInfo));

  // Capture the VTable after patching.
  PROC vtable[kFunctionCount];
  memcpy(vtable, GetVtable(&factory_), sizeof(vtable));

  // Patch it again, this should be idempotent.
  EXPECT_HRESULT_SUCCEEDED(
      PatchInterfaceMethods(&factory_, IClassFactory_PatchInfo));

  // Should not have changed the VTable on second call.
  EXPECT_EQ(0, memcmp(vtable, GetVtable(&factory_), sizeof(vtable)));
}

namespace vtable_patch {
// Expose internal implementation detail, purely for testing.
extern base::Lock patch_lock_;

}  // namespace vtable_patch

TEST_F(VtablePatchManagerTest, ThreadSafePatching) {
  // It's difficult to test for threadsafe patching, but as a close proxy,
  // test for no patching happening from a background thread while the patch
  // lock is held.
  base::Thread background("Background Test Thread");

  EXPECT_TRUE(background.Start());
  base::win::ScopedHandle event(::CreateEvent(NULL, TRUE, FALSE, NULL));

  // Grab the patch lock.
  vtable_patch::patch_lock_.Acquire();

  // Instruct the background thread to patch factory_.
  background.message_loop()->PostTask(FROM_HERE,
      NewRunnableFunction(&vtable_patch::PatchInterfaceMethods,
                          &factory_,
                          &IClassFactory_PatchInfo[0]));

  // And subsequently to signal the event. Neither of these actions should
  // occur until we've released the patch lock.
  background.message_loop()->PostTask(FROM_HERE,
      NewRunnableFunction(::SetEvent, event.Get()));

  // Wait for a little while, to give the background thread time to process.
  // We expect this wait to time out, as the background thread should end up
  // blocking on the patch lock.
  EXPECT_EQ(WAIT_TIMEOUT, ::WaitForSingleObject(event.Get(), 50));

  // Verify that patching did not take place yet.
  EXPECT_CALL(factory_, LockServer(TRUE))
      .WillOnce(Return(S_FALSE));
  EXPECT_EQ(S_FALSE, factory_.LockServer(TRUE));

  // Release the lock and wait on the event again to ensure
  // the patching has taken place now.
  vtable_patch::patch_lock_.Release();
  EXPECT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(event.Get(), INFINITE));

  // We should not get called here anymore.
  EXPECT_CALL(factory_, LockServer(TRUE))
      .Times(0);

  // But should be diverted here.
  EXPECT_CALL(*this, LockServerPatch(_, &factory_, TRUE))
      .WillOnce(Return(S_FALSE));
  EXPECT_EQ(S_FALSE, factory_.LockServer(TRUE));

  // Same deal for unpatching.
  ::ResetEvent(event.Get());

  // Grab the patch lock.
  vtable_patch::patch_lock_.Acquire();

  // Instruct the background thread to unpatch.
  background.message_loop()->PostTask(FROM_HERE,
      NewRunnableFunction(&vtable_patch::UnpatchInterfaceMethods,
                          &IClassFactory_PatchInfo[0]));

  // And subsequently to signal the event. Neither of these actions should
  // occur until we've released the patch lock.
  background.message_loop()->PostTask(FROM_HERE,
      NewRunnableFunction(::SetEvent, event.Get()));

  // Wait for a little while, to give the background thread time to process.
  // We expect this wait to time out, as the background thread should end up
  // blocking on the patch lock.
  EXPECT_EQ(WAIT_TIMEOUT, ::WaitForSingleObject(event.Get(), 50));

  // We should still be patched.
  EXPECT_CALL(factory_, LockServer(TRUE))
      .Times(0);
  EXPECT_CALL(*this, LockServerPatch(_, &factory_, TRUE))
      .WillOnce(Return(S_FALSE));
  EXPECT_EQ(S_FALSE, factory_.LockServer(TRUE));

  // Release the patch lock and wait on the event.
  vtable_patch::patch_lock_.Release();
  EXPECT_EQ(WAIT_OBJECT_0, ::WaitForSingleObject(event.Get(), INFINITE));

  // Verify that unpatching took place.
  EXPECT_CALL(factory_, LockServer(TRUE))
      .WillOnce(Return(S_FALSE));
  EXPECT_EQ(S_FALSE, factory_.LockServer(TRUE));
}
