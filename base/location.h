// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_LOCATION_H_
#define BASE_LOCATION_H_

#include <stddef.h>

#include <cassert>
#include <string>

#include "base/base_export.h"
#include "base/debug/debugging_flags.h"
#include "base/hash.h"

namespace base {

// Location provides basic info where of an object was constructed, or was
// significantly brought to life.
class BASE_EXPORT Location {
 public:
  Location();
  Location(const Location& other);

  // Only initializes the file name and program counter, the source information
  // will be null for the strings, and -1 for the line number.
  // TODO(http://crbug.com/760702) remove file name from this constructor.
  Location(const char* file_name, const void* program_counter);

  // Constructor should be called with a long-lived char*, such as __FILE__.
  // It assumes the provided value will persist as a global constant, and it
  // will not make a copy of it.
  Location(const char* function_name,
           const char* file_name,
           int line_number,
           const void* program_counter);

  // Comparator for hash map insertion. The program counter should uniquely
  // identify a location.
  bool operator==(const Location& other) const {
    return program_counter_ == other.program_counter_;
  }

  // Returns true if there is source code location info. If this is false,
  // the Location object only contains a program counter or is
  // default-initialized (the program counter is also null).
  bool has_source_info() const { return function_name_ && file_name_; }

  // Will be nullptr for default initialized Location objects and when source
  // names are disabled.
  const char* function_name() const { return function_name_; }

  // Will be nullptr for default initialized Location objects and when source
  // names are disabled.
  const char* file_name() const { return file_name_; }

  // Will be -1 for default initialized Location objects and when source names
  // are disabled.
  int line_number() const { return line_number_; }

  // The address of the code generating this Location object. Should always be
  // valid except for default initialized Location objects, which will be
  // nullptr.
  const void* program_counter() const { return program_counter_; }

  // See Write().
  std::string ToString() const;

  // Translate the some of the state in this instance into a human readable
  // string with HTML characters in the function names escaped, and append that
  // string to |output|. Inclusion of the file_name_ and function_name_ are
  // optional, and controlled by the boolean arguments.
  //
  // If the function and file names are null, the program counter will be
  // written.
  void Write(bool display_filename, bool display_function_name,
             std::string* output) const;

 private:
  const char* function_name_ = nullptr;
  const char* file_name_ = nullptr;
  int line_number_ = -1;
  const void* program_counter_ = nullptr;
};

// A "snapshotted" representation of the Location class that can safely be
// passed across process boundaries.
struct BASE_EXPORT LocationSnapshot {
  // The default constructor is exposed to support the IPC serialization macros.
  LocationSnapshot();
  explicit LocationSnapshot(const Location& location);
  ~LocationSnapshot();

  std::string file_name;
  std::string function_name;
  int line_number = -1;
};

BASE_EXPORT const void* GetProgramCounter();

// The macros defined here will expand to the current function.
#if BUILDFLAG(ENABLE_LOCATION_SOURCE)

// Full source information should be included.
#define FROM_HERE FROM_HERE_WITH_EXPLICIT_FUNCTION(__func__)
#define FROM_HERE_WITH_EXPLICIT_FUNCTION(function_name) \
  ::base::Location(function_name, __FILE__, __LINE__,   \
                   ::base::GetProgramCounter())

#else

// TODO(http://crbug.com/760702) remove the __FILE__ argument from these calls.
#define FROM_HERE ::base::Location(__FILE__, ::base::GetProgramCounter())
#define FROM_HERE_WITH_EXPLICIT_FUNCTION(function_name) \
  ::base::Location(function_name, __FILE__, -1, ::base::GetProgramCounter())

#endif

}  // namespace base

namespace tracked_objects {

// TODO(http://crbug.com/763556): Convert all uses of Location and
// GetProgramCounter to use the base namespace and remove these lines.
using ::base::Location;
using ::base::GetProgramCounter;

}  // namespace tracked_objects

namespace std {

// Specialization for using Location in hash tables.
template <>
struct hash<::base::Location> {
  std::size_t operator()(const ::base::Location& loc) const {
    const void* program_counter = loc.program_counter();
    return base::Hash(&program_counter, sizeof(void*));
  }
};

}  // namespace std

#endif  // BASE_LOCATION_H_
