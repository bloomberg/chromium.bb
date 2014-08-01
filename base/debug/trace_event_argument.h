// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_DEBUG_TRACE_EVENT_ARGUMENT_H_
#define BASE_DEBUG_TRACE_EVENT_ARGUMENT_H_

#include <string>
#include <vector>

#include "base/debug/trace_event.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class DictionaryValue;
class ListValue;
class Value;

namespace debug {

class BASE_EXPORT TracedValue : public ConvertableToTraceFormat {
 public:
  TracedValue();

  void EndDictionary();
  void EndArray();

  void SetInteger(const char* name, int value);
  void SetDouble(const char* name, double);
  void SetBoolean(const char* name, bool value);
  void SetString(const char* name, const std::string& value);
  void SetValue(const char* name, Value* value);
  void BeginDictionary(const char* name);
  void BeginArray(const char* name);

  void AppendInteger(int);
  void AppendDouble(double);
  void AppendBoolean(bool);
  void AppendString(const std::string&);
  void BeginArray();
  void BeginDictionary();

  virtual void AppendAsTraceFormat(std::string* out) const OVERRIDE;

 private:
  virtual ~TracedValue();

  DictionaryValue* GetCurrentDictionary();
  ListValue* GetCurrentArray();

  scoped_ptr<base::Value> root_;
  std::vector<Value*> stack_;
  DISALLOW_COPY_AND_ASSIGN(TracedValue);
};

}  // namespace debug
}  // namespace base

#endif  // BASE_DEBUG_TRACE_EVENT_ARGUMENT_H_
