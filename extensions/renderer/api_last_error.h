// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_LAST_ERROR_H_
#define EXTENSIONS_RENDERER_API_LAST_ERROR_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "v8/include/v8.h"

namespace extensions {

// Handles creating and clearing a 'lastError' property to hold the last error
// set by an extension API function.
class APILastError {
 public:
  // Returns the object the 'lastError' property should be exposed on for the
  // given context.
  using GetParent =
      base::Callback<v8::Local<v8::Object>(v8::Local<v8::Context>)>;

  explicit APILastError(const GetParent& get_parent);
  APILastError(APILastError&& other);
  ~APILastError();

  // Sets the last error for the given |context| to |error|.
  void SetError(v8::Local<v8::Context> context, const std::string& error);

  // Clears the last error in the given |context|. If |report_if_unchecked| is
  // true and the developer didn't check the error, this throws an exception.
  void ClearError(v8::Local<v8::Context> context, bool report_if_unchecked);

 private:
  GetParent get_parent_;

  DISALLOW_COPY_AND_ASSIGN(APILastError);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_LAST_ERROR_H_
