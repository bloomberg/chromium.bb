// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(COMPILER_MSVC)
// MSDN says to #include <intrin.h>, but that breaks the VS2005 build.
extern "C" {
void* _ReturnAddress();
}
#endif

#include "base/tracked.h"

#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/tracked_objects.h"

using base::TimeTicks;

namespace tracked_objects {

//------------------------------------------------------------------------------

Location::Location(const char* function_name,
                   const char* file_name,
                   int line_number,
                   const void* program_counter)
    : function_name_(function_name),
      file_name_(file_name),
      line_number_(line_number),
      program_counter_(program_counter) {
}

Location::Location()
    : function_name_("Unknown"),
      file_name_("Unknown"),
      line_number_(-1),
      program_counter_(NULL) {
}

std::string Location::ToString() const {
  return std::string(function_name_) + "@" + file_name_ + ":" +
      base::IntToString(line_number_);
}

void Location::Write(bool display_filename, bool display_function_name,
                     std::string* output) const {
  base::StringAppendF(output, "%s[%d] ",
      display_filename ? file_name_ : "line",
      line_number_);

  if (display_function_name) {
    WriteFunctionName(output);
    output->push_back(' ');
  }
}

void Location::WriteFunctionName(std::string* output) const {
  // Translate "<" to "&lt;" for HTML safety.
  // TODO(jar): Support ASCII or html for logging in ASCII.
  for (const char *p = function_name_; *p; p++) {
    switch (*p) {
      case '<':
        output->append("&lt;");
        break;

      case '>':
        output->append("&gt;");
        break;

      default:
        output->push_back(*p);
        break;
    }
  }
}

#if defined(COMPILER_MSVC)
__declspec(noinline)
#endif
BASE_EXPORT const void* GetProgramCounter() {
#if defined(COMPILER_MSVC)
  return _ReturnAddress();
#elif defined(COMPILER_GCC)
  return __builtin_extract_return_addr(__builtin_return_address(0));
#endif  // COMPILER_GCC

  return NULL;
}

//------------------------------------------------------------------------------

#if !defined(TRACK_ALL_TASK_OBJECTS)

Tracked::Tracked() : birth_program_counter_(NULL) {}
Tracked::~Tracked() {}

void Tracked::SetBirthPlace(const Location& from_here) {
  birth_program_counter_ = from_here.program_counter();
}

const Location Tracked::GetBirthPlace() const {
  static Location kNone("NoFunctionName", "NeedToSetBirthPlace", -1, NULL);
  return kNone;
}
bool Tracked::MissingBirthPlace() const { return false; }
void Tracked::ResetBirthTime() {}

#else

Tracked::Tracked()
    : tracked_births_(NULL),
      tracked_birth_time_(TimeTicks::Now()) {
}

Tracked::~Tracked() {
  if (!ThreadData::IsActive() || !tracked_births_)
    return;
  ThreadData::current()->TallyADeath(*tracked_births_,
                                     TimeTicks::Now() - tracked_birth_time_);
}

void Tracked::SetBirthPlace(const Location& from_here) {
  if (!ThreadData::IsActive())
    return;
  if (tracked_births_)
    tracked_births_->ForgetBirth();
  ThreadData* current_thread_data = ThreadData::current();
  if (!current_thread_data)
    return;  // Shutdown started, and this thread wasn't registered.
  tracked_births_ = current_thread_data->TallyABirth(from_here);

  birth_program_counter_ = from_here.program_counter();
}

const Location Tracked::GetBirthPlace() const {
  static Location kNone("UnknownFunctionName", "UnknownFile", -1, NULL);
  return tracked_births_ ? tracked_births_->location() : kNone;
}

void Tracked::ResetBirthTime() {
  tracked_birth_time_ = TimeTicks::Now();
}

bool Tracked::MissingBirthPlace() const {
  return  !tracked_births_ || tracked_births_->location().line_number() == -1;
}

#endif  // !defined(TRACK_ALL_TASK_OBJECTS)

}  // namespace tracked_objects
