// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_RESOURCE_CONVERTER_H
#define CONTENT_RENDERER_PEPPER_RESOURCE_CONVERTER_H

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_loop_proxy.h"

namespace content {

// This class is responsible for converting V8 vars to Pepper resources.
class ResourceConverter {
 public:
  ResourceConverter();
  ~ResourceConverter();
  // ShutDown must be called before any vars created by the ResourceConverter
  // are valid. It handles creating any resource hosts that need to be created.
  // |message_loop_proxy| is the message loop to run the callback from.
  void ShutDown(
      const base::Callback<void(bool)>& callback,
      const scoped_refptr<base::MessageLoopProxy>& message_loop_proxy);

 private:
  DISALLOW_COPY_AND_ASSIGN(ResourceConverter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_RESOURCE_CONVERTER_H
