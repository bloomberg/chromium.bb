// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ERROR_CONSOLE_EXTENSION_ERROR_H_
#define CHROME_BROWSER_EXTENSIONS_ERROR_CONSOLE_EXTENSION_ERROR_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/strings/string16.h"

namespace extensions {

class ExtensionError {
 public:
  enum Type {
    MANIFEST_PARSING_ERROR,
    JAVASCRIPT_RUNTIME_ERROR
  };

  virtual ~ExtensionError();

  virtual std::string PrintForTest() const;

  Type type() const { return type_; }
  const base::string16& source() const { return source_; }
  const base::string16& message() const { return message_; }
  const std::string& extension_id() const { return extension_id_; }
  bool from_incognito() const { return from_incognito_; }

 protected:
  ExtensionError(Type type,
                 bool from_incognito,
                 const base::string16& source,
                 const base::string16& message);

  // Which type of error this is.
  Type type_;
  // Whether or not the error was caused while incognito.
  bool from_incognito_;
  // The source for the error; this can be a script, web page, or manifest file.
  // This is stored as a string (rather than a url) since it can be a Chrome
  // script file (e.g., event_bindings.js).
  base::string16 source_;
  // The error message itself.
  base::string16 message_;
  // The ID of the extension which caused the error. This may be absent, since
  // we can't always know the id (such as when a manifest fails to parse).
  std::string extension_id_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionError);
};

class ManifestParsingError : public ExtensionError {
 public:
  ManifestParsingError(bool from_incognito,
                       const base::string16& source,
                       const base::string16& message,
                       size_t line_number);
  virtual ~ManifestParsingError();

  virtual std::string PrintForTest() const OVERRIDE;

  size_t line_number() const { return line_number_; }
 private:
  size_t line_number_;

  DISALLOW_COPY_AND_ASSIGN(ManifestParsingError);
};

class JavascriptRuntimeError : public ExtensionError {
 public:
  struct StackFrame {
    size_t line_number;
    size_t column_number;
    // This is stored as a string (rather than a url) since it can be a
    // Chrome script file (e.g., event_bindings.js).
    base::string16 url;
    base::string16 function;  // optional

    // STL-Required constructor
    StackFrame();

    StackFrame(size_t frame_line,
               size_t frame_column,
               const base::string16& frame_url,
               const base::string16& frame_function  /* can be empty */);

    ~StackFrame();
  };
  typedef std::vector<StackFrame> StackTrace;

  JavascriptRuntimeError(bool from_incognito,
                         const base::string16& source,
                         const base::string16& message,
                         logging::LogSeverity level,
                         const base::string16& details);
  virtual ~JavascriptRuntimeError();

  virtual std::string PrintForTest() const OVERRIDE;

  logging::LogSeverity level() const { return level_; }
  const base::string16& execution_context_url() const {
      return execution_context_url_;
  }
  const StackTrace& stack_trace() const { return stack_trace_; }
 private:
  // Parse the JSON |details| passed to the error. This includes a stack trace
  // and an execution context url.
  void ParseDetails(const base::string16& details);
  // Try to determine the ID of the extension. This may be obtained through the
  // reported source, or through the execution context url.
  void DetermineExtensionID();

  logging::LogSeverity level_;
  base::string16 execution_context_url_;
  StackTrace stack_trace_;

  DISALLOW_COPY_AND_ASSIGN(JavascriptRuntimeError);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ERROR_CONSOLE_EXTENSION_ERROR_H_
