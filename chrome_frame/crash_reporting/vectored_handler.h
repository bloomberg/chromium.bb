// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_CRASH_REPORTING_VECTORED_HANDLER_H_
#define CHROME_FRAME_CRASH_REPORTING_VECTORED_HANDLER_H_


#if !defined(_M_IX86)
#error only x86 is supported for now.
#endif

// Create dump policy:
// 1. Scan SEH chain, if there is a handler/filter that belongs to our
//    module - assume we expect this one and hence do nothing here.
// 2. If the address of the exception is in our module - create dump.
// 3. If our module is in somewhere in callstack - create dump.
// The E class is supposed to provide external/API functions. Using template
// make testability easier. It shall confirm the following concept/archetype:
//struct E {
//  void WriteDump(EXCEPTION_POINTERS* p) {
//  }
//
//  // Used mainly to ignore exceptions from IsBadRead/Write/Ptr.
//  bool ShouldIgnoreException(const EXCEPTION_POINTERS* exptr) {
//    return 0;
//  }
//
//  // Retrieve the SEH list head.
//  EXCEPTION_REGISTRATION_RECORD* RtlpGetExceptionList() {
//    return NULL;
//  }
//
//  // Get the stack trace as correctly as possible.
//  WORD RtlCaptureStackBackTrace(DWORD FramesToSkip, DWORD FramesToCapture,
//                                void** BackTrace, DWORD* BackTraceHash) {
//      return 0;
//  }
//
//  // Check whether the stack guard page is in place.
//  bool CheckForStackOverflow(EXCEPTION_POINTERS* p) {
//    return 0;
//  }
//
//  bool IsOurModule(const void* address) {
//    return 0;
//  }
//};
// The methods shall be placed in .text$veh_m
template <typename E>
class VectoredHandlerT {
 public:
  VectoredHandlerT(E* api);
  ~VectoredHandlerT();

  // TODO(stoyan): Come with better way to skip initial stack frames.
  FORCEINLINE LONG Handler(EXCEPTION_POINTERS* exceptionInfo);
  long get_exceptions_seen() const {
    return exceptions_seen_;
  }

 private:
  bool ModuleHasInstalledSEHFilter();
  E* api_;
  long exceptions_seen_;
};

// Maintains start and end address of a single module of interest. If we want
// do check for multiple modules, this class has to be extended to support a
// list of modules (DLLs).
struct ModuleOfInterest {
  // The callback from VectoredHandlerT::Handler().
  inline bool IsOurModule(const void* address) {
    return (start_ <= address && address < end_);
  }

  // Helpers.
  inline void SetModule(const void* module_start, const void* module_end) {
    start_ = module_start;
    end_ = module_end;
  }

  inline void SetCurrentModule() {
    // Find current module boundaries.
    const void* start = &__ImageBase;
    const char* s = reinterpret_cast<const char*>(start);
    const IMAGE_NT_HEADERS32* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>
        (s + __ImageBase.e_lfanew);
    const void* end = s + nt->OptionalHeader.SizeOfImage;
    SetModule(start, end);
  }

  const void* start_;
  const void* end_;
};

struct ModuleOfInterestWithExcludedRegion : public ModuleOfInterest {
  inline bool IsOurModule(const void* address) {
    return (start_ <= address && address < end_) &&
           (address < special_region_start_ || special_region_end_ <= address);
  }

  inline void SetExcludedRegion(const void* start, const void* end) {
    special_region_start_ = start;
    special_region_end_ = end;
  }

  const void* special_region_start_;
  const void* special_region_end_;
};


#endif  // CHROME_FRAME_CRASH_REPORTING_VECTORED_HANDLER_H_
