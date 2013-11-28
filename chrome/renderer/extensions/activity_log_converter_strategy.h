// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_ACTIVITY_LOG_CONVERTER_STRATEGY_H_
#define CHROME_RENDERER_EXTENSIONS_ACTIVITY_LOG_CONVERTER_STRATEGY_H_

#include "base/compiler_specific.h"
#include "content/public/renderer/v8_value_converter.h"
#include "v8/include/v8.h"

namespace extensions {

// This class is used by activity logger and extends behavior of
// V8ValueConverter. It overwrites conversion of V8 arrays and objects.  When
// converting arrays and objects, we must not invoke any JS code which may
// result in triggering activity logger. In such case, the log entries will be
// generated due to V8 object conversion rather than extension activity.
class ActivityLogConverterStrategy
    : public content::V8ValueConverter::Strategy {
 public:
  virtual ~ActivityLogConverterStrategy() {}

  virtual bool FromV8Object(v8::Handle<v8::Object> value,
                            base::Value** out,
                            v8::Isolate* isolate) const OVERRIDE;
  virtual bool FromV8Array(v8::Handle<v8::Array> value,
                           base::Value** out,
                           v8::Isolate* isolate) const OVERRIDE;

 private:
  bool FromV8ObjectInternal(v8::Handle<v8::Object> value,
                            base::Value** out,
                            v8::Isolate* isolate) const;
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_ACTIVITY_LOG_CONVERTER_STRATEGY_H_
