// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DEBUG_TRACED_VALUE_H_
#define CC_DEBUG_TRACED_VALUE_H_

#include <string>

#include "base/debug/trace_event.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class DictionaryValue;
class Value;
}
namespace cc {

class TracedValue : public base::debug::ConvertableToTraceFormat {
 public:
  static scoped_ptr<base::Value> CreateIDRef(const void* id);
  static void MakeDictIntoImplicitSnapshot(
      base::DictionaryValue* dict, const char* object_name, const void* id);
  static void MakeDictIntoImplicitSnapshotWithCategory(
      const char* category,
      base::DictionaryValue* dict,
      const char* object_name,
      const void* id);
  static scoped_ptr<ConvertableToTraceFormat> FromValue(
      base::Value* value);

  virtual ~TracedValue();

  virtual void AppendAsTraceFormat(std::string* out) const OVERRIDE;

 private:
  explicit TracedValue(base::Value* value);

  scoped_ptr<base::Value> value_;

  DISALLOW_COPY_AND_ASSIGN(TracedValue);
};

}  // namespace cc

#endif  // CC_DEBUG_TRACED_VALUE_H_
