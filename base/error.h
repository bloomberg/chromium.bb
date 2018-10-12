// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ERROR_H_
#define BASE_ERROR_H_

#include <ostream>
#include <string>
#include <utility>

#include "base/macros.h"

namespace openscreen {

// Represents an error returned by an OSP library operation.  An error has a
// code and an optional message.
class Error {
 public:
  enum class Code {
    // No error occurred.
    kNone = 0,
    // CBOR parsing error.
    kCborParsing = 1,

    // Presentation start errors.
    kNoAvailableScreens,
    kRequestCancelled,
    kNoPresentationFound,
    kPreviousStartInProgress,
    kUnknownStartError,
  };

  Error();
  Error(const Error& error);
  Error(Error&& error) noexcept;
  explicit Error(Code code);
  Error(Code code, const std::string& message);
  Error(Code code, std::string&& message);

  Error& operator=(const Error& other);
  Error& operator=(Error&& other);
  bool operator==(const Error& other) const;

  Code code() const { return code_; }
  const std::string& message() const { return message_; }

  static std::string CodeToString(Error::Code code);

 private:
  Code code_ = Code::kNone;
  std::string message_;
};

std::ostream& operator<<(std::ostream& out, const Error& error);

// A convenience function to return a single value from a function that can
// return a value or an error.  For normal results, construct with a Value*
// (ErrorOr takes ownership) and the Error will be kNone with an empty message.
// For Error results, construct with an error code and value.
//
// Example:
//
// ErrorOr<Bar> Foo::DoSomething() {
//   if (success) {
//     return Bar();
//   } else {
//     return Error(kBadThingHappened, "No can do");
//   }
// }
//
// TODO(mfoltz):
// - Add support for type conversions
// - Better support for primitive (non-movable) values
template <typename Value>
class ErrorOr {
 public:
  ErrorOr(ErrorOr&& error_or) = default;
  ErrorOr(Value&& value) noexcept : value_(value) {}
  ErrorOr(Error error) : error_(std::move(error)) {}
  ErrorOr(Error::Code code) : error_(code) {}
  ErrorOr(Error::Code code, std::string message)
      : error_(code, std::move(message)) {}
  ~ErrorOr() = default;

  ErrorOr& operator=(ErrorOr&& other) = default;

  bool is_error() const { return error_.code() != Error::Code::kNone; }
  bool is_value() const { return !is_error(); }
  const Error& error() const { return error_; }

  Error&& MoveError() { return std::move(error_); }

  const Value& value() const { return value_; }

  Value&& MoveValue() { return std::move(value_); }

 private:
  Error error_;
  Value value_;

  DISALLOW_COPY_AND_ASSIGN(ErrorOr);
};

}  // namespace openscreen

#endif  // BASE_ERROR_H_
