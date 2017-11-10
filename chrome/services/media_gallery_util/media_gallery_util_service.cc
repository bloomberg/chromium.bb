// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/media_gallery_util/media_gallery_util_service.h"

#include "build/build_config.h"
#include "chrome/services/media_gallery_util/media_parser.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace chrome {

namespace {

void OnMediaParserRequest(
    service_manager::ServiceContextRefFactory* ref_factory,
    mojom::MediaParserRequest request) {
  mojo::MakeStrongBinding(
      std::make_unique<MediaParser>(ref_factory->CreateRef()),
      std::move(request));
}

}  // namespace

MediaGalleryUtilService::MediaGalleryUtilService() = default;

MediaGalleryUtilService::~MediaGalleryUtilService() = default;

std::unique_ptr<service_manager::Service>
MediaGalleryUtilService::CreateService() {
  return std::make_unique<MediaGalleryUtilService>();
}

void MediaGalleryUtilService::OnStart() {
  ref_factory_ = std::make_unique<service_manager::ServiceContextRefFactory>(
      base::Bind(&service_manager::ServiceContext::RequestQuit,
                 base::Unretained(context())));
  registry_.AddInterface(base::Bind(&OnMediaParserRequest, ref_factory_.get()));
}

void MediaGalleryUtilService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

}  //  namespace chrome
