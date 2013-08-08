// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_error.h"

#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/constants.h"
#include "url/gurl.h"

using base::string16;

namespace extensions {

namespace {

const char kLineNumberKey[] = "lineNumber";
const char kColumnNumberKey[] = "columnNumber";
const char kURLKey[] = "url";
const char kFunctionNameKey[] = "functionName";
const char kExecutionContextURLKey[] = "executionContextURL";
const char kStackTraceKey[] = "stackTrace";

// Try to retrieve an extension ID from a |url|. On success, returns true and
// populates |extension_id| with the ID. On failure, returns false and leaves
// extension_id untouched.
bool GetExtensionIDFromGURL(const GURL& url, std::string* extension_id) {
  if (url.SchemeIs(kExtensionScheme)) {
    *extension_id = url.host();
    return true;
  }
  return false;
}

}  // namespace

ExtensionError::ExtensionError(Type type,
                               const std::string& extension_id,
                               bool from_incognito,
                               const string16& source,
                               const string16& message)
    : type_(type),
      extension_id_(extension_id),
      from_incognito_(from_incognito),
      source_(source),
      message_(message) {
}

ExtensionError::~ExtensionError() {
}

std::string ExtensionError::PrintForTest() const {
  return std::string("Extension Error:") +
         "\n  OTR:     " + std::string(from_incognito_ ? "true" : "false") +
         "\n  Source:  " + base::UTF16ToUTF8(source_) +
         "\n  Message: " + base::UTF16ToUTF8(message_) +
         "\n  ID:      " + extension_id_;
}

ManifestParsingError::ManifestParsingError(const std::string& extension_id,
                                           const string16& message)
    : ExtensionError(ExtensionError::MANIFEST_PARSING_ERROR,
                     extension_id,
                     false,  // extensions can't be installed while incognito.
                     base::FilePath(kManifestFilename).AsUTF16Unsafe(),
                     message) {
}

ManifestParsingError::~ManifestParsingError() {
}

std::string ManifestParsingError::PrintForTest() const {
  return ExtensionError::PrintForTest() +
         "\n  Type:    ManifestParsingError";
}

JavascriptRuntimeError::StackFrame::StackFrame() : line_number(-1),
                                                   column_number(-1) {
}

JavascriptRuntimeError::StackFrame::StackFrame(size_t frame_line,
                                               size_t frame_column,
                                               const string16& frame_url,
                                               const string16& frame_function)
    : line_number(frame_line),
      column_number(frame_column),
      url(frame_url),
      function(frame_function) {
}

JavascriptRuntimeError::StackFrame::~StackFrame() {
}

JavascriptRuntimeError::JavascriptRuntimeError(bool from_incognito,
                                               const string16& source,
                                               const string16& message,
                                               logging::LogSeverity level,
                                               const string16& details)
    : ExtensionError(ExtensionError::JAVASCRIPT_RUNTIME_ERROR,
                     std::string(),  // We don't know the id yet.
                     from_incognito,
                     source,
                     message),
      level_(level) {
  ParseDetails(details);
  DetermineExtensionID();
}

JavascriptRuntimeError::~JavascriptRuntimeError() {
}

std::string JavascriptRuntimeError::PrintForTest() const {
  std::string result = ExtensionError::PrintForTest() +
         "\n  Type:    JavascriptRuntimeError"
         "\n  Context: " + base::UTF16ToUTF8(execution_context_url_) +
         "\n  Stack Trace: ";
  for (StackTrace::const_iterator iter = stack_trace_.begin();
       iter != stack_trace_.end(); ++iter) {
    result += "\n    {"
              "\n      Line:     " + base::IntToString(iter->line_number) +
              "\n      Column:   " + base::IntToString(iter->column_number) +
              "\n      URL:      " + base::UTF16ToUTF8(iter->url) +
              "\n      Function: " + base::UTF16ToUTF8(iter->function) +
              "\n    }";
  }
  return result;
}

void JavascriptRuntimeError::ParseDetails(const string16& details) {
  scoped_ptr<base::Value> value(
      base::JSONReader::Read(base::UTF16ToUTF8(details)));
  const base::DictionaryValue* details_value;
  const base::ListValue* trace_value = NULL;

  // The |details| value should contain an execution context url and a stack
  // trace.
  if (!value.get() ||
      !value->GetAsDictionary(&details_value) ||
      !details_value->GetString(kExecutionContextURLKey,
                                &execution_context_url_) ||
      !details_value->GetList(kStackTraceKey, &trace_value)) {
    NOTREACHED();
    return;
  }

  int line = 0;
  int column = 0;
  string16 url;

  for (size_t i = 0; i < trace_value->GetSize(); ++i) {
    const base::DictionaryValue* frame_value = NULL;
    CHECK(trace_value->GetDictionary(i, &frame_value));

    frame_value->GetInteger(kLineNumberKey, &line);
    frame_value->GetInteger(kColumnNumberKey, &column);
    frame_value->GetString(kURLKey, &url);

    string16 function;
    frame_value->GetString(kFunctionNameKey, &function);  // This can be empty.
    stack_trace_.push_back(StackFrame(line, column, url, function));
  }
}

void JavascriptRuntimeError::DetermineExtensionID() {
  if (!GetExtensionIDFromGURL(GURL(source_), &extension_id_))
    GetExtensionIDFromGURL(GURL(execution_context_url_), &extension_id_);
}

}  // namespace extensions
