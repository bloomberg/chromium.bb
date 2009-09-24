// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CHROME_FRAME_FUNCTION_STUB_H_
#define CHROME_FRAME_FUNCTION_STUB_H_

#include <windows.h>
#include "base/logging.h"

// IMPORTANT: The struct below must be byte aligned.
#pragma pack(push)
#pragma pack(1)

#ifndef _M_IX86
#error Only x86 supported right now.
#endif

extern "C" IMAGE_DOS_HEADER __ImageBase;

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
 private:
  typedef enum AsmConstants {
    POP_EAX = 0x58,
    PUSH = 0x68,
    PUSH_EAX = 0x50,
    JUMP_RELATIVE = 0xE9
  };

  FunctionStub(uintptr_t extra_argument, void* dest)
      : signature_(reinterpret_cast<HMODULE>(&__ImageBase)) {
    Opcodes::Hook& hook = code_.hook_;
    hook.pop_return_addr_ = POP_EAX;
    hook.push_ = PUSH;
    hook.arg_ = extra_argument;
    hook.push_return_addr_ = PUSH_EAX;
    hook.jump_to_ = JUMP_RELATIVE;

    // Calculate the target jump to the destination function.
    hook.target_ = CalculateRelativeJump(dest, &hook.jump_to_);

    // Allow the process to execute this struct as code.
    DWORD old_protect = 0;
    // Allow reads too since we want read-only member variable access at
    // all times.
    ::VirtualProtect(this, sizeof(FunctionStub), PAGE_EXECUTE_READ,
                     &old_protect);
  }

  ~FunctionStub() {
    // No more execution rights.
    DWORD old_protect = 0;
    ::VirtualProtect(this, sizeof(FunctionStub), PAGE_READWRITE, &old_protect);
  }

  // Calculates the target value for a relative jump.
  // The function assumes that the size of the opcode is 1 byte.
  inline uintptr_t CalculateRelativeJump(const void* absolute_to,
                                         const void* absolute_from) const {
    return (reinterpret_cast<uintptr_t>(absolute_to) -
            reinterpret_cast<uintptr_t>(absolute_from)) - 
           (sizeof(uint8) + sizeof(uintptr_t));
  }

  // Does the opposite of what CalculateRelativeJump does, which is to
  // calculate back the absolute address that previously was relative to
  // some other address.
  inline uintptr_t CalculateAbsoluteAddress(const void* relative_to,
                                            uintptr_t relative_address) const {
    return relative_address + sizeof(uint8) + sizeof(uintptr_t) +
           reinterpret_cast<uintptr_t>(relative_to);
  }

  // Used to identify function stubs that belong to this module.
  HMODULE signature_;

  // IMPORTANT: Do not change the order of member variables
  union Opcodes {
    // Use this struct when the stub forwards the call to our hook function
    // providing an extra argument.
    struct Hook {
      uint8 pop_return_addr_;  // pop eax
      uint8 push_;             // push arg  ; push...
      uintptr_t arg_;          //           ; extra argument
      uint8 push_return_addr_; // push eax  ; push the return address
      uint8 jump_to_;          // jmp       ; jump...
      uintptr_t target_;       //           ; to the hook function
    } hook_;
    // When the stub is bypassed, we jump directly to a given target,
    // usually the originally hooked function.
    struct Bypassed {
      uint8 jump_to_;          // jmp to
      uintptr_t target_;       // relative target.
    } bypassed_;
  };

  Opcodes code_;

 public:
  // Neutralizes this stub and converts it to a direct jump to a new target.
  void BypassStub(void* new_target) {
    DWORD old_protect = 0;
    // Temporarily allow us to write to member variables
    ::VirtualProtect(this, sizeof(FunctionStub), PAGE_EXECUTE_READWRITE,
                     &old_protect);

    // Now, just change the first 5 bytes to jump directly to the new target.
    Opcodes::Bypassed& bypassed = code_.bypassed_;
    bypassed.jump_to_ = JUMP_RELATIVE;
    bypassed.target_ = CalculateRelativeJump(new_target, &bypassed.jump_to_);

    // Restore the previous protection flags.
    ::VirtualProtect(this, sizeof(FunctionStub), old_protect, &old_protect);

    // Flush the instruction cache just in case.
    ::FlushInstructionCache(::GetCurrentProcess(), this, sizeof(FunctionStub));
  }

  // @returns the argument supplied in the call to @ref Create
  inline uintptr_t argument() const {
    DCHECK(code_.hook_.pop_return_addr_ == POP_EAX) << "stub has been disabled";
    return code_.hook_.arg_;
  }

  inline bool is_bypassed() const {
    return code_.bypassed_.jump_to_ == JUMP_RELATIVE;
  }

  inline uintptr_t absolute_target() const {
    DCHECK(code_.hook_.pop_return_addr_ == POP_EAX) << "stub has been disabled";
    return CalculateAbsoluteAddress(
        reinterpret_cast<const void*>(&code_.hook_.jump_to_),
        code_.hook_.target_);
  }

  // Returns true if the stub is valid and enabled.
  // Don't call this method after bypassing the stub.
  inline bool is_valid() const {
    return signature_ == reinterpret_cast<HMODULE>(&__ImageBase) &&
        code_.hook_.pop_return_addr_ == POP_EAX;
  }

  inline PROC code() const {
    return reinterpret_cast<PROC>(const_cast<Opcodes*>(&code_));
  }

  // Use to create a new function stub as shown above.
  //
  // @param extra_argument The static argument to pass to the function.
  // @param dest Target function to which the stub applies.
  // @returns NULL if an error occurs, otherwise a pointer to the
  // function stub.
  //
  // TODO(tommi): Change this so that VirtualAlloc isn't called for
  // every stub.  Instead we should re-use each allocation for
  // multiple stubs.  In practice I'm guessing that there would
  // only be one allocation per process, since each allocation actually
  // allocates at least one page of memory (4K).  Size of FunctionStub
  // is 12 bytes, so one page could house 341 function stubs.
  // When stubs are created frequently, VirtualAlloc could fail
  // and last error is ERROR_NOT_ENOUGH_MEMORY (8).
  static FunctionStub* Create(uintptr_t extra_argument, void* dest) {
    DCHECK(dest);

    // Use VirtualAlloc to get memory for the stub.  This gives us a
    // whole page that we don't share with anyone else.
    // Initially the memory must be READWRITE to allow for construction
    // PAGE_EXECUTE is set in constructor.
    FunctionStub* ret = reinterpret_cast<FunctionStub*>(VirtualAlloc(NULL, 
        sizeof(FunctionStub), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));

    if (!ret) {
      NOTREACHED();
    } else {
      // Construct
      ret->FunctionStub::FunctionStub(extra_argument, dest);
    }

    return ret;
  }

  static FunctionStub* FromCode(void* address) {
    Opcodes* code = reinterpret_cast<Opcodes*>(address);
    if (code->hook_.pop_return_addr_ == POP_EAX) {
      FunctionStub* stub = reinterpret_cast<FunctionStub*>(
          reinterpret_cast<uint8*>(address) - sizeof(HMODULE));
      if (stub->is_valid())
        return stub;
    }

    return NULL;
  }

  // Deallocates a FunctionStub.  The stub must not be in use on any thread!
  static bool Destroy(FunctionStub* stub) {
    DCHECK(stub != NULL);
    FunctionStub* to_free = reinterpret_cast<FunctionStub*>(stub);
    to_free->FunctionStub::~FunctionStub();
    BOOL success = VirtualFree(to_free, sizeof(FunctionStub), MEM_DECOMMIT);
    DCHECK(success) << "VirtualFree";
    return success != FALSE;
  }
};

#pragma pack(pop)

#endif  // CHROME_FRAME_FUNCTION_STUB_H_
