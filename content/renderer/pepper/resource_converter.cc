// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/resource_converter.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"

namespace content {

ResourceConverter::ResourceConverter() {
}

ResourceConverter::~ResourceConverter() {
}

void ResourceConverter::ShutDown(
    const base::Callback<void(bool)>& callback,
    const scoped_refptr<base::MessageLoopProxy>& message_loop_proxy) {
  // TODO(raymes): Implement the creation of a browser resource host here.
  message_loop_proxy->PostTask(FROM_HERE, base::Bind(callback, true));
}

}  // namespace content
