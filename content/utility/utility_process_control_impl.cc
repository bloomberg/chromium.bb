// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/utility_process_control_impl.h"

#include "base/bind.h"
#include "content/common/mojo/static_application_loader.h"
#include "content/public/common/content_client.h"
#include "content/public/utility/content_utility_client.h"
#include "content/public/utility/utility_thread.h"
#include "content/utility/utility_thread_impl.h"

#if defined(ENABLE_MOJO_MEDIA_IN_UTILITY_PROCESS)
#include "media/mojo/services/mojo_media_application_factory.h"
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
    NameToLoaderMap* name_to_loader_map) {
  NameToLoaderMap& map_ref = *name_to_loader_map;

  ContentUtilityClient::StaticMojoApplicationMap apps;
  GetContentClient()->utility()->RegisterMojoApplications(&apps);

  for (const auto& entry : apps) {
    map_ref[entry.first] = new StaticApplicationLoader(
        entry.second, base::Bind(&QuitProcess));
  }

#if defined(ENABLE_MOJO_MEDIA_IN_UTILITY_PROCESS)
  map_ref["mojo:media"] = new StaticApplicationLoader(
      base::Bind(&media::CreateMojoMediaApplication), base::Bind(&QuitProcess));
#endif
}

void UtilityProcessControlImpl::OnLoadFailed() {
  UtilityThreadImpl* utility_thread =
      static_cast<UtilityThreadImpl*>(UtilityThread::Get());
  utility_thread->Shutdown();
  utility_thread->ReleaseProcessIfNeeded();
}

}  // namespace content
