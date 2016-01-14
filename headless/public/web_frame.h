// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_WEB_FRAME_H_
#define HEADLESS_PUBLIC_WEB_FRAME_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/values.h"
#include "headless/public/headless_export.h"

namespace headless {

class WebDocument;

// Class representing a frame in a web page (e.g. main frame or an iframe).
// Should be accessed from renderer main thread.
class HEADLESS_EXPORT WebFrame {
 public:
  virtual ~WebFrame() {}

  using ScriptExecutionCallback =
      base::Callback<void(const std::vector<scoped_ptr<base::Value>>&)>;

  // Schedule given script for execution.
  virtual void ExecuteScript(const std::string& source_code) = 0;

  // Execute given script and return its result.
  // Returned value will be converted to json (base::Value).
  // Special effects to bear in mind:
  // - Boolean will be converted to base::FundamentalValue (no surprises here).
  // - Number will be converted to base::FundamentalValue.
  // - Array will be converted to base::ListValue.
  //   Note: All non-numerical properties will be omitted
  //   (e.g. "array = [1, 2, 3]; array['property'] = 'value'; return array"
  //    will return [1, 2, 3]).
  // - Object will be converted to base::DictionaryValue
  //   Note: Only string can be key in base::DictionaryValue, so all non-string
  //   properties will be omitted
  //   (e.g. "obj = Object(); obj['key'] = 'value'; obj[0] = 42;" will return
  //    {"key":"value"}).
  virtual void ExecuteScriptAndReturnValue(
      const std::string& source_code,
      const ScriptExecutionCallback& callback) = 0;

  virtual std::string ContentAsText(size_t max_chars) const = 0;
  virtual std::string ContentAsMarkup() const = 0;
  virtual proto::Document ContentAsProtobuf() const = 0;

  virtual gfx::Size GetScrollOffset() const = 0;
  virtual void SetScrollOffset(const gfx::Size& offset) = 0;

  virtual float GetPageScaleFactor() const = 0;
  virtual void SetPageScaleFactor(float page_scale_factor) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebFrame);
};

}  // namespace headless

#endif  // HEADLESS_PUBLIC_WEB_FRAME_H_
