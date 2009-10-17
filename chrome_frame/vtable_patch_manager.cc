// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/vtable_patch_manager.h"

#include <algorithm>

#include "base/logging.h"
#include "base/scoped_ptr.h"

#include "chrome_frame/function_stub.h"

namespace vtable_patch {

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

  for (MethodPatchInfo* it = patches; it->index_ != -1; ++it) {
    if (it->stub_ != NULL) {
      // If this DCHECK fires it means that we are using the same VTable
      // information to patch two different interfaces.
      DCHECK(false);
      DLOG(ERROR) << "Attempting to patch two different VTables with the "
                  << "same VTable information";
      continue;
    }

    PROC original_fn = vtable[it->index_];
    FunctionStub* stub = FunctionStub::FromCode(original_fn);
    if (stub != NULL) {
      DLOG(ERROR) << "attempt to patch a function that's already patched";
      DCHECK(stub->absolute_target() ==
             reinterpret_cast<uintptr_t>(it->method_)) <<
             "patching the same method multiple times with different hooks?";
      continue;
    }

    stub = FunctionStub::Create(reinterpret_cast<uintptr_t>(original_fn),
                                it->method_);
    if (!stub) {
      NOTREACHED();
      return E_OUTOFMEMORY;
    } else {
      DWORD protect = 0;
      if (::VirtualProtect(&vtable[it->index_], sizeof(PROC),
                           PAGE_EXECUTE_READWRITE, &protect)) {
        it->stub_ = stub;  // save the stub
        vtable[it->index_] = stub->code();
        ::VirtualProtect(&vtable[it->index_], sizeof(PROC), protect,
                         &protect);
      } else {
        NOTREACHED();
      }
    }
  }

  return S_OK;
}

HRESULT UnpatchInterfaceMethods(MethodPatchInfo* patches) {
  for (MethodPatchInfo* it = patches; it->index_ != -1; ++it) {
    if (it->stub_) {
      DCHECK(it->stub_->absolute_target() ==
          reinterpret_cast<uintptr_t>(it->method_));
      // Modify the stub to just jump directly to the original function.
      it->stub_->BypassStub(reinterpret_cast<void*>(it->stub_->argument()));
      it->stub_ = NULL;
      // Leave the stub in memory so that we won't break any possible chains.
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
