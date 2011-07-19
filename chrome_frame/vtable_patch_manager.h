// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_VTABLE_PATCH_MANAGER_H_
#define CHROME_FRAME_VTABLE_PATCH_MANAGER_H_

#include <windows.h>

#include <list>

#include "base/synchronization/lock.h"

struct FunctionStub;

// This namespace provides methods to patch VTable methods of COM interfaces.
namespace vtable_patch {

// Internal implementation, exposed only for testing.
namespace internal {

// Replaces *entry with new_proc iff *entry is curr_proc.
// Returns true iff *entry was rewritten.
// Note: does not crash on access violation.
bool ReplaceFunctionPointer(void** entry, void* new_proc, void* curr_proc);

}  // namespace internal

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

// Disabled as we're not using it atm.
#if 0
// Used when dynamically patching zero or more (usually more than 1)
// implementations of a particular interface.
class DynamicPatchManager {
 public:
  explicit DynamicPatchManager(const MethodPatchInfo* patch_prototype);
  ~DynamicPatchManager();

  // Returns S_OK if the object was successfully patched, S_FALSE if it was
  // already patched or an error value if something bad happened.
  HRESULT PatchObject(void* unknown);

  bool UnpatchAll();

 protected:
  struct PatchedObject {
    void* vtable_;
    MethodPatchInfo patch_info_[1];

    // Used to match PatchedObject instances based on the vtable when
    // searching through the patch list.
    bool operator==(const PatchedObject& that) const {
      return vtable_ == that.vtable_;
    }
  };

  typedef std::list<PatchedObject*> PatchList;
  const MethodPatchInfo* patch_prototype_;
  mutable base::Lock patch_list_lock_;
  PatchList patch_list_;
};
#endif  // disable DynamicPatchManager

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

#define DCHECK_IS_NOT_PATCHED(IFName) \
    for (vtable_patch::MethodPatchInfo* it = IFName##_PatchInfo; \
         it->index_ != -1; ++it) { \
      DCHECK(it->stub_ == NULL); \
    }

#define DCHECK_IS_PATCHED(IFName) \
    for (vtable_patch::MethodPatchInfo* it = IFName##_PatchInfo; \
         it->index_ != -1; ++it) { \
      DCHECK(it->stub_ != NULL); \
    }

// Checks if the interface is patched.  Note that only the first method
// is checked and subsequent methods are assumed to have the same state.
#define IS_PATCHED(IFName) \
  (IFName##_PatchInfo[0].stub_ != NULL)

// Ends the declaration of a VTable patch by adding an entry with
// index set to -1.
#define END_VTABLE_PATCHES() \
      -1, NULL, NULL \
    };

#endif  // CHROME_FRAME_VTABLE_PATCH_MANAGER_H_
