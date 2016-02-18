// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_MEDIA_APPLICATION_FACTORY_H_
#define MEDIA_MOJO_SERVICES_MOJO_MEDIA_APPLICATION_FACTORY_H_

#include "base/memory/scoped_ptr.h"
#include "mojo/shell/public/cpp/shell_client.h"

namespace media {

// Creates a MojoMediaApplication instance using the default MojoMediaClient.
scoped_ptr<mojo::ShellClient> CreateMojoMediaApplication();

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_MEDIA_APPLICATION_FACTORY_H_
