// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/vtable_patch_manager.h"

#include "base/logging.h"

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

}  // namespace vtable_patch
