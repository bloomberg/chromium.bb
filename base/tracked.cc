// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/tracked.h"

#include "base/stringprintf.h"
#include "base/tracked_objects.h"

using base::TimeTicks;

namespace tracked_objects {

//------------------------------------------------------------------------------

Location::Location(const char* function_name, const char* file_name,
                   int line_number)
    : function_name_(function_name),
      file_name_(file_name),
      line_number_(line_number) {
}

Location::Location()
    : function_name_("Unknown"),
      file_name_("Unknown"),
      line_number_(-1) {
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

//------------------------------------------------------------------------------

#ifndef TRACK_ALL_TASK_OBJECTS

Tracked::Tracked() {}
Tracked::~Tracked() {}
void Tracked::SetBirthPlace(const Location& from_here) {}
const Location Tracked::GetBirthPlace() const {
  static Location kNone("NoFunctionName", "NeedToSetBirthPlace", -1);
  return kNone;
}
bool Tracked::MissingBirthplace() const { return false; }
void Tracked::ResetBirthTime() {}

#else

Tracked::Tracked()
    : tracked_births_(NULL),
      tracked_birth_time_(TimeTicks::Now()) {
  if (!ThreadData::IsActive())
    return;
  SetBirthPlace(Location("NoFunctionName", "NeedToSetBirthPlace", -1));
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
}

const Location Tracked::GetBirthPlace() const {
  return tracked_births_->location();
}

void Tracked::ResetBirthTime() {
  tracked_birth_time_ = TimeTicks::Now();
}

bool Tracked::MissingBirthplace() const {
  return -1 == tracked_births_->location().line_number();
}

#endif  // NDEBUG

}  // namespace tracked_objects
