// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/utility_process_control_impl.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "content/public/common/content_client.h"
#include "content/public/utility/content_utility_client.h"
#include "content/public/utility/utility_thread.h"
#include "mojo/shell/static_application_loader.h"
#include "url/gurl.h"

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

UtilityProcessControlImpl::UtilityProcessControlImpl() {
  ContentUtilityClient::StaticMojoApplicationMap apps;
  GetContentClient()->utility()->RegisterMojoApplications(&apps);
  for (const auto& entry : apps) {
    url_to_loader_map_[entry.first] = new mojo::shell::StaticApplicationLoader(
        entry.second, base::Bind(&QuitProcess));
  }

#if defined(ENABLE_MOJO_MEDIA_IN_UTILITY_PROCESS)
  url_to_loader_map_[media::MojoMediaApplication::AppUrl()] =
      new mojo::shell::StaticApplicationLoader(
          base::Bind(&media::MojoMediaApplication::CreateApp),
          base::Bind(&QuitProcess));
#endif
}

UtilityProcessControlImpl::~UtilityProcessControlImpl() {
  STLDeleteValues(&url_to_loader_map_);
}

void UtilityProcessControlImpl::LoadApplication(
    const mojo::String& url,
    mojo::InterfaceRequest<mojo::Application> request,
    const LoadApplicationCallback& callback) {
  GURL application_url = GURL(url.To<std::string>());
  auto it = url_to_loader_map_.find(application_url);
  if (it == url_to_loader_map_.end()) {
    callback.Run(false);
    return;
  }

  callback.Run(true);
  it->second->Load(application_url, request.Pass());
}

}  // namespace content
