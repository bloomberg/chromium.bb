// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/resource_converter.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"

namespace content {

ResourceConverter::~ResourceConverter() {
}

ResourceConverterImpl::ResourceConverterImpl() {
}

ResourceConverterImpl::~ResourceConverterImpl() {
}

void ResourceConverterImpl::ShutDown(
    const base::Callback<void(bool)>& callback) {
  // TODO(raymes): Implement the creation of a browser resource host here.
  base::MessageLoop::current()->PostTask(FROM_HERE, base::Bind(callback, true));
}

}  // namespace content
