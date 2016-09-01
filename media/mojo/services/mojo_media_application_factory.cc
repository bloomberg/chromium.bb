// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_media_application_factory.h"

#include "base/memory/ptr_util.h"
#include "media/mojo/services/mojo_media_application.h"

#if defined(OS_ANDROID)
#include "media/mojo/services/android_mojo_media_client.h"  // nogncheck
using DefaultClient = media::AndroidMojoMediaClient;
#else
#include "media/mojo/services/default_mojo_media_client.h"  // nogncheck
using DefaultClient = media::DefaultMojoMediaClient;
#endif

namespace media {

// static
std::unique_ptr<shell::Service> CreateMojoMediaApplication(
    const base::Closure& quit_closure) {
  return std::unique_ptr<shell::Service>(new MojoMediaApplication(
      base::MakeUnique<DefaultClient>(), quit_closure));
}

}  // namespace media
