// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BINDINGS_BINDING_ACCESS_CHECKER_H_
#define EXTENSIONS_RENDERER_BINDINGS_BINDING_ACCESS_CHECKER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "v8/include/v8.h"

namespace extensions {

// A helper class to handle access-checking API features.
class BindingAccessChecker {
 public:
  // The callback for determining if a given API feature (specified by |name|)
  // is available in the given context.
  using AvailabilityCallback =
      base::RepeatingCallback<bool(v8::Local<v8::Context>,
                                   const std::string& name)>;

  BindingAccessChecker(AvailabilityCallback is_available);
  ~BindingAccessChecker();

  // Returns true if the feature specified by |full_name| is available to the
  // given |context|.
  bool HasAccess(v8::Local<v8::Context> context,
                 const std::string& full_name) const;

  // Same as HasAccess(), but throws an exception in the |context| if it doesn't
  // have access.
  bool HasAccessOrThrowError(v8::Local<v8::Context> context,
                             const std::string& full_name) const;

 private:
  AvailabilityCallback is_available_;

  DISALLOW_COPY_AND_ASSIGN(BindingAccessChecker);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDINGS_BINDING_ACCESS_CHECKER_H_
