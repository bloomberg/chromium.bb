// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CHROME_FRAME_FUNCTION_STUB_H_
#define CHROME_FRAME_FUNCTION_STUB_H_

#include <windows.h>

#include "base/basictypes.h"

// IMPORTANT: The struct below must be byte aligned.
#pragma pack(push)
#pragma pack(1)

struct FunctionStubAsm {
  // The stub always starts with an indirect jump, which starts out jumping
  // to the remainder of the stub. This means we can bypass the stub by
  // rewriting the jump destination, which is data, in a manner that does
  // not involve writing code, only writing data at a natural word boundary.
  uint16 jump_to_bypass_;        // indirect jump
  uintptr_t bypass_target_addr_; // to the bypass target.
  uint8 pop_return_addr_;        // pop eax
  uint16 push_;                  // push [arg]   ; push...
  uintptr_t arg_addr_;           //              ; extra argument
  uint8 push_return_addr_;       // push eax     ; push the return address
  uint16 jump_to_target;         // jmp [target] ; jump...
  uintptr_t target_addr_;        //              ; to the hook function
};

#pragma pack(pop)


#ifndef _M_IX86
#error Only x86 supported right now.
#endif

// This struct is assembly code + signature.  The purpose of the struct is to be
// able to hook an existing function with our own and store information such
// as the original function pointer with the code stub.  Typically this is used
// for patching entries of a vtable or e.g. a globally registered wndproc
// for a class as opposed to a window.
// When unhooking, you can just call the BypassStub() function and leave the
// stub in memory.  This unhooks your function while leaving the (potential)
// chain of patches intact.
//
// @note: This class is meant for __stdcall calling convention and
//        it uses eax as a temporary variable.  The struct can
//        be improved in the future to save eax before the
//        operation and then restore it.
//
// For instance if the function prototype is:
//
// @code
//   LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
// @endcode
//
// and we would like to add one static argument to make it, say:
//
// @code
//   LRESULT MyNewWndProc(WNDPROC original, HWND hwnd, UINT msg,
//                        WPARAM wparam, LPARAM lparam);
// @endcode
//
// That can be achieved by wrapping the function up with a FunctionStub:
//
// @code
//   FunctionStub* stub = FunctionStub::Create(original_wndproc, MyNewWndProc);
//   SetClassLongPtr(wnd, GCLP_WNDPROC, stub->code());
// @endcode
struct FunctionStub {
 public:
  // Neutralizes this stub and converts it to a direct jump to a new target.
  void BypassStub(void* new_target);

  inline bool is_bypassed() const {
    return bypass_address_ !=
        reinterpret_cast<uintptr_t>(&stub_.pop_return_addr_);
  }

  // Returns true if the stub is valid and enabled.
  // Don't call this method after bypassing the stub.
  bool is_valid() const;

  inline PROC code() const {
    return reinterpret_cast<PROC>(const_cast<FunctionStubAsm*>(&stub_));
  }

  // Use to create a new function stub as shown above.
  // @param extra_argument The static argument to pass to the function.
  // @param dest Target function to which the stub applies.
  // @returns NULL if an error occurs, otherwise a pointer to the
  //    function stub.
  static FunctionStub* Create(uintptr_t extra_argument, void* dest);

  // Test whether address (likely) points to an existing function stub.
  // @returns NULL if address does not point to a function stub.
  // @note likely means approximately 1/2^48 here.
  static FunctionStub* FromCode(void* address);

  // Deallocates a FunctionStub.
  // The stub must not be in use on any thread!
  static bool Destroy(FunctionStub* stub);

  // Accessors.
  uintptr_t argument() const { return argument_; }
  void set_argument(uintptr_t argument) { argument_ = argument; }

  uintptr_t bypass_address() const { return bypass_address_; }
  void set_bypass_address(uintptr_t bypass_address) {
    bypass_address_ = bypass_address;
  }

  uintptr_t destination_function() const { return destination_function_; }
  void set_destination_function(uintptr_t destination_function) {
    destination_function_ = destination_function;
  }

 protected:
  // Protected for testing only.
  FunctionStub(uintptr_t extra_argument, void* dest);
  ~FunctionStub();

  void Init(FunctionStubAsm* stub);

  FunctionStubAsm stub_;

  // Used to identify function stubs that belong to this module.
  HMODULE signature_;

  // This is the argument value that gets passed to the destination_function_.
  uintptr_t argument_;
  // Bypass address, if this is the address of the pop_return_addr_, the
  // function stub is not bypassed.
  uintptr_t bypass_address_;
  // The destination function we dispatch to, not used if the stub
  // is bypassed.
  uintptr_t destination_function_;
};

#endif  // CHROME_FRAME_FUNCTION_STUB_H_
