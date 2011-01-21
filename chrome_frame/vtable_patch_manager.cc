// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/vtable_patch_manager.h"

#include <atlcomcli.h>

#include <algorithm>

#include "base/atomicops.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "chrome_frame/function_stub.h"
#include "chrome_frame/utils.h"

namespace vtable_patch {

// The number of times we retry a patch/unpatch operation in case of
// VM races with other 3rd party software trying to patch the same thing.
const int kMaxRetries = 3;

// We hold a lock over all patching operations to make sure that we don't
// e.g. race on VM operations to the same patches, or to physical pages
// shared across different VTABLEs.
base::Lock patch_lock_;

namespace internal {
// Because other parties in our process might be attempting to patch the same
// virtual tables at the same time, we have a race to modify the VM protections
// on the pages. We also need to do a compare/swap type operation when we
// modify the function, so as to be sure that we grab the most recent value.
// Hence the SEH blocks and the nasty-looking compare/swap operation.
bool ReplaceFunctionPointer(void** entry, void* new_proc, void* curr_proc) {
  __try {
    base::subtle::Atomic32 prev_value;

    prev_value = base::subtle::NoBarrier_CompareAndSwap(
        reinterpret_cast<base::subtle::Atomic32 volatile*>(entry),
        reinterpret_cast<base::subtle::Atomic32>(curr_proc),
        reinterpret_cast<base::subtle::Atomic32>(new_proc));

    return curr_proc == reinterpret_cast<void*>(prev_value);
  } __except(EXCEPTION_EXECUTE_HANDLER) {
    // Oops, we took exception on access.
  }

  return false;
}

}  // namespace

// Convenient definition of a VTABLE
typedef PROC* Vtable;

// Returns a pointer to the VTable of a COM interface.
// @param unknown [in] The pointer of the COM interface.
inline Vtable GetIFVTable(void* unknown) {
  return reinterpret_cast<Vtable>(*reinterpret_cast<void**>(unknown));
}

HRESULT PatchInterfaceMethods(void* unknown, MethodPatchInfo* patches) {
  // Do some sanity checking of the input arguments.
  if (NULL == unknown || NULL == patches) {
    NOTREACHED();
    return E_INVALIDARG;
  }

  Vtable vtable = GetIFVTable(unknown);
  DCHECK(vtable);

  // All VM operations, patching and manipulation of MethodPatchInfo
  // is done under a global lock, to ensure multiple threads don't
  // race, whether on an individual patch, or on VM operations to
  // the same physical pages.
  base::AutoLock lock(patch_lock_);

  for (MethodPatchInfo* it = patches; it->index_ != -1; ++it) {
    if (it->stub_ != NULL) {
      // If this DCHECK fires it means that we are using the same VTable
      // information to patch two different interfaces, or we've lost a
      // race with another thread who's patching the same interface.
      DLOG(WARNING) << "Attempting to patch two different VTables with the "
          "same VTable information, or patching the same interface on "
          "multiple threads";
      continue;
    }

    PROC original_fn = vtable[it->index_];
    FunctionStub* stub = NULL;

#ifndef NDEBUG
    stub = FunctionStub::FromCode(original_fn);
    if (stub != NULL) {
      DLOG(ERROR) << "attempt to patch a function that's already patched";
      DCHECK(stub->destination_function() ==
             reinterpret_cast<uintptr_t>(it->method_)) <<
             "patching the same method multiple times with different hooks?";
      continue;
    }
#endif

    stub = FunctionStub::Create(reinterpret_cast<uintptr_t>(original_fn),
                                it->method_);
    if (!stub) {
      NOTREACHED();
      return E_OUTOFMEMORY;
    }

    // Do the VM operations and the patching in a loop, to try and ensure
    // we succeed even if there's a VM operation or a patch race against
    // other 3rd parties patching.
    bool succeeded = false;
    for (int i = 0; !succeeded && i < kMaxRetries; ++i) {
      DWORD protect = 0;
      if (!::VirtualProtect(&vtable[it->index_], sizeof(PROC),
                            PAGE_EXECUTE_READWRITE, &protect)) {
        HRESULT hr = AtlHresultFromLastError();
        DLOG(ERROR) << "VirtualProtect failed 0x" << std::hex << hr;

        // Go around again in the feeble hope that this is
        // a temporary problem.
        continue;
      }
      original_fn = vtable[it->index_];
      stub->set_argument(reinterpret_cast<uintptr_t>(original_fn));
      succeeded = internal::ReplaceFunctionPointer(
          reinterpret_cast<void**>(&vtable[it->index_]), stub->code(),
          original_fn);

      if (!::VirtualProtect(&vtable[it->index_], sizeof(PROC), protect,
                            &protect)) {
        DLOG(ERROR) << "VirtualProtect failed to restore protection";
      }
    }

    if (!succeeded) {
      FunctionStub::Destroy(stub);
      stub = NULL;

      DLOG(ERROR) << "Failed to patch VTable.";
      return E_FAIL;
    } else {
      // Success, save the stub we created.
      it->stub_ = stub;
      PinModule();
    }
  }

  return S_OK;
}

HRESULT UnpatchInterfaceMethods(MethodPatchInfo* patches) {
  base::AutoLock lock(patch_lock_);

  for (MethodPatchInfo* it = patches; it->index_ != -1; ++it) {
    if (it->stub_) {
      DCHECK(it->stub_->destination_function() ==
          reinterpret_cast<uintptr_t>(it->method_));
      // Modify the stub to just jump directly to the original function.
      it->stub_->BypassStub(reinterpret_cast<void*>(it->stub_->argument()));
      it->stub_ = NULL;
      // Leave the stub in memory so that we won't break any possible chains.

      // TODO(siggi): why not restore the original VTBL pointer here, provided
      //    we haven't been chained?
    } else {
      DLOG(WARNING) << "attempt to unpatch a function that wasn't patched";
    }
  }

  return S_OK;
}

// Disabled for now as we're not using it atm.
#if 0

DynamicPatchManager::DynamicPatchManager(const MethodPatchInfo* patch_prototype)
    : patch_prototype_(patch_prototype) {
  DCHECK(patch_prototype_);
  DCHECK(patch_prototype_->stub_ == NULL);
}

DynamicPatchManager::~DynamicPatchManager() {
  UnpatchAll();
}

HRESULT DynamicPatchManager::PatchObject(void* unknown) {
  int patched_methods = 0;
  for (; patch_prototype_[patched_methods].index_ != -1; patched_methods++) {
    // If you hit this, then you are likely using the prototype instance for
    // patching in _addition_ to this class.  This is not a good idea :)
    DCHECK(patch_prototype_[patched_methods].stub_ == NULL);
  }

  // Prepare a new patch object using the patch info from the prototype.
  int mem_size = sizeof(PatchedObject) +
                 sizeof(MethodPatchInfo) * patched_methods;
  PatchedObject* entry = reinterpret_cast<PatchedObject*>(new char[mem_size]);
  entry->vtable_ = GetIFVTable(unknown);
  memcpy(entry->patch_info_, patch_prototype_,
         sizeof(MethodPatchInfo) * (patched_methods + 1));

  patch_list_lock_.Acquire();

  // See if we've already patched this vtable before.
  // The search is done via the == operator of the PatchedObject class.
  PatchList::const_iterator it = std::find(patch_list_.begin(),
                                           patch_list_.end(), entry);
  HRESULT hr;
  if (it == patch_list_.end()) {
    hr = PatchInterfaceMethods(unknown, entry->patch_info_);
    if (SUCCEEDED(hr)) {
      patch_list_.push_back(entry);
      entry = NULL;  // Ownership transferred to the array.
    }
  } else {
    hr = S_FALSE;
  }

  patch_list_lock_.Release();

  delete entry;

  return hr;
}

bool DynamicPatchManager::UnpatchAll() {
  patch_list_lock_.Acquire();
  PatchList::iterator it;
  for (it = patch_list_.begin(); it != patch_list_.end(); it++) {
    UnpatchInterfaceMethods((*it)->patch_info_);
    delete (*it);
  }
  patch_list_.clear();
  patch_list_lock_.Release();

  return true;
}

#endif  // disabled DynamicPatchManager

}  // namespace vtable_patch
