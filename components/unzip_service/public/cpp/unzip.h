// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNZIP_SERVICE_PUBLIC_CPP_UNZIP_H_
#define COMPONENTS_UNZIP_SERVICE_PUBLIC_CPP_UNZIP_H_

#include "base/callback_forward.h"

namespace base {
class FilePath;
}

namespace service_manager {
class Connector;
}

namespace unzip {

// Unzips |zip_file| into |output_dir|.
using UnzipCallback = base::OnceCallback<void(bool result)>;
// TODO(waffles): Unzip can only be called on blockable threads (never UI).
void Unzip(service_manager::Connector* connector,
           const base::FilePath& zip_file,
           const base::FilePath& output_dir,
           UnzipCallback result_callback);

}  // namespace unzip

#endif  // COMPONENTS_UNZIP_SERVICE_PUBLIC_CPP_UNZIP_H_
