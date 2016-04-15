// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/utility_process_control_impl.h"

#include "base/bind.h"
#include "content/public/common/content_client.h"
#include "content/public/utility/content_utility_client.h"
#include "content/public/utility/utility_thread.h"
#include "content/utility/utility_thread_impl.h"

#if defined(ENABLE_MOJO_MEDIA_IN_UTILITY_PROCESS)
#include "media/mojo/services/mojo_media_application_factory.h"
#endif

namespace content {

UtilityProcessControlImpl::UtilityProcessControlImpl() {}

UtilityProcessControlImpl::~UtilityProcessControlImpl() {}

void UtilityProcessControlImpl::RegisterApplicationFactories(
    ApplicationFactoryMap* factories) {
  ContentUtilityClient::StaticMojoApplicationMap apps;
  GetContentClient()->utility()->RegisterMojoApplications(&apps);
  for (const auto& entry : apps)
    factories->insert(std::make_pair(entry.first, entry.second));

#if defined(ENABLE_MOJO_MEDIA_IN_UTILITY_PROCESS)
  factories->insert(std::make_pair(
      "mojo:media", base::Bind(&media::CreateMojoMediaApplication)));
#endif
}

void UtilityProcessControlImpl::OnApplicationQuit() {
  UtilityThread::Get()->ReleaseProcessIfNeeded();
}

void UtilityProcessControlImpl::OnLoadFailed() {
  UtilityThreadImpl* utility_thread =
      static_cast<UtilityThreadImpl*>(UtilityThread::Get());
  utility_thread->Shutdown();
  utility_thread->ReleaseProcessIfNeeded();
}

}  // namespace content
