// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_COMMON_VTABLE_PATCH_MANAGER_H_
#define CHROME_FRAME_COMMON_VTABLE_PATCH_MANAGER_H_

#include <windows.h>

struct FunctionStub;
// This namespace provides methods to patch VTable methods of COM interfaces.
namespace vtable_patch {

// This structure represents information about one VTable method.
// We allocate an array of these structures per VTable that we patch to
// remember the original method. We also use this structure to actually
// describe the VTable patch functions
struct MethodPatchInfo {
  int index_;
  PROC method_;
  FunctionStub* stub_;
};

// Patches methods in the passed in COM interface. The indexes of the
// methods to patch and the actual patch functions are described in the
// array pointed to by patches.
// @param[in] unknown  The pointer of the COM interface to patch
// @param[in] patches  An array of MethodPatchInfo structures describing
//  the methods to patch and the patch functions.
//  The last entry of patches must have index_ set to -1.
HRESULT PatchInterfaceMethods(void* unknown, MethodPatchInfo* patches);

// Using the patch info provided in |patches| the function goes through the
// list of patched methods and modifies thunks so that they no longer point
// to a hook method but rather go straight through to the original target.
// The thunk itself is not destroyed to support chaining.
// @param[in] patches  An array of MethodPatchInfo structures describing
//  the methods to patch and the patch functions.
//  The last entry of patches must have index_ set to -1.
HRESULT UnpatchInterfaceMethods(MethodPatchInfo* patches);

}  // namespace vtable_patch

// Begins the declaration of a VTable patch
// @param IFName The name of the interface to patch
#define BEGIN_VTABLE_PATCHES(IFName) \
    vtable_patch::MethodPatchInfo IFName##_PatchInfo[] = {

// Defines a single method patch in a VTable
// @param index The index of the method to patch
// @param PatchFunction The patch function
#define VTABLE_PATCH_ENTRY(index, PatchFunction) {\
      index, \
      reinterpret_cast<PROC>(PatchFunction), \
      NULL, \
    },

// Ends the declaration of a VTable patch by adding an entry with
// index set to -1.
#define END_VTABLE_PATCHES() \
      -1, NULL, NULL \
    };

#endif // CHROME_FRAME_COMMON_VTABLE_PATCH_MANAGER_H_
