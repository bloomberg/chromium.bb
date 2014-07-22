// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NATIVE_VIEWPORT_SERVICE_H_
#define MOJO_SERVICES_NATIVE_VIEWPORT_SERVICE_H_

#include "base/memory/scoped_vector.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/services/native_viewport/native_viewport_export.h"
#include "mojo/shell/context.h"

namespace mojo {
namespace services {

MOJO_NATIVE_VIEWPORT_EXPORT mojo::ApplicationImpl*
    CreateNativeViewportService(
        ScopedMessagePipeHandle service_provider_handle);

}  // namespace services
}  // namespace mojo

#endif  // MOJO_SERVICES_NATIVE_VIEWPORT_SERVICE_H_
