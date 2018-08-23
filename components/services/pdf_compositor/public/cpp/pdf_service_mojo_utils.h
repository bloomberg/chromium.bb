// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_PDF_COMPOSITOR_PUBLIC_CPP_PDF_SERVICE_MOJO_UTILS_H_
#define COMPONENTS_SERVICES_PDF_COMPOSITOR_PUBLIC_CPP_PDF_SERVICE_MOJO_UTILS_H_

#include <memory>

#include "mojo/public/cpp/system/buffer.h"

namespace base {
class SharedMemory;
}  // namespace base

namespace printing {

std::unique_ptr<base::SharedMemory> GetShmFromMojoHandle(
    mojo::ScopedSharedBufferHandle handle);

}  // namespace printing

#endif  // COMPONENTS_SERVICES_PDF_COMPOSITOR_PUBLIC_CPP_PDF_SERVICE_MOJO_UTILS_H_
