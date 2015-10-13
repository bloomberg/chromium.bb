// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/utility_process_control_impl.h"

#include "base/bind.h"
#include "content/public/common/content_client.h"
#include "content/public/utility/content_utility_client.h"
#include "content/public/utility/utility_thread.h"
#include "content/utility/utility_thread_impl.h"
#include "mojo/shell/static_application_loader.h"

#if defined(ENABLE_MOJO_MEDIA_IN_UTILITY_PROCESS)
#include "media/mojo/services/mojo_media_application.h"
#endif

namespace content {

namespace {

// Called when a static application terminates.
void QuitProcess() {
  UtilityThread::Get()->ReleaseProcessIfNeeded();
}

}  // namespace

UtilityProcessControlImpl::UtilityProcessControlImpl() {}

UtilityProcessControlImpl::~UtilityProcessControlImpl() {}

void UtilityProcessControlImpl::RegisterApplicationLoaders(
    URLToLoaderMap* url_to_loader_map) {
  URLToLoaderMap& map_ref = *url_to_loader_map;

  ContentUtilityClient::StaticMojoApplicationMap apps;
  GetContentClient()->utility()->RegisterMojoApplications(&apps);

  for (const auto& entry : apps) {
    map_ref[entry.first] = new mojo::shell::StaticApplicationLoader(
        entry.second, base::Bind(&QuitProcess));
  }

#if defined(ENABLE_MOJO_MEDIA_IN_UTILITY_PROCESS)
  map_ref[GURL("mojo:media")] = new mojo::shell::StaticApplicationLoader(
      base::Bind(&media::MojoMediaApplication::CreateApp),
      base::Bind(&QuitProcess));
#endif
}

void UtilityProcessControlImpl::OnLoadFailed() {
  UtilityThreadImpl* utility_thread =
      static_cast<UtilityThreadImpl*>(UtilityThread::Get());
  utility_thread->Shutdown();
  utility_thread->ReleaseProcessIfNeeded();
}

}  // namespace content
