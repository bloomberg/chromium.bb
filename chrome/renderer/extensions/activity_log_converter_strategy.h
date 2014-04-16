// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_ACTIVITY_LOG_CONVERTER_STRATEGY_H_
#define CHROME_RENDERER_EXTENSIONS_ACTIVITY_LOG_CONVERTER_STRATEGY_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
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
  typedef content::V8ValueConverter::Strategy::FromV8ValueCallback
      FromV8ValueCallback;

  ActivityLogConverterStrategy();
  virtual ~ActivityLogConverterStrategy();

  // content::V8ValueConverter::Strategy implementation.
  virtual bool FromV8Object(v8::Handle<v8::Object> value,
                            base::Value** out,
                            v8::Isolate* isolate,
                            const FromV8ValueCallback& callback) const OVERRIDE;
  virtual bool FromV8Array(v8::Handle<v8::Array> value,
                           base::Value** out,
                           v8::Isolate* isolate,
                           const FromV8ValueCallback& callback) const OVERRIDE;

  void set_enable_detailed_parsing(bool enable_detailed_parsing) {
    enable_detailed_parsing_ = enable_detailed_parsing;
  }

 private:
  bool FromV8Internal(v8::Handle<v8::Object> value,
                      base::Value** out,
                      v8::Isolate* isolate,
                      const FromV8ValueCallback& callback) const;

  // Whether or not to extract a detailed value from the passed in objects. We
  // do this when we need more information about the arguments in order to
  // determine, e.g., if an ad was injected.
  bool enable_detailed_parsing_;

  DISALLOW_COPY_AND_ASSIGN(ActivityLogConverterStrategy);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_ACTIVITY_LOG_CONVERTER_STRATEGY_H_
