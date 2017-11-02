// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/file_util_service.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/services/file_util/zip_file_creator.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace chrome {

namespace {

void OnZipFileCreatorRequest(
    service_manager::ServiceContextRefFactory* ref_factory,
    chrome::mojom::ZipFileCreatorRequest request) {
  mojo::MakeStrongBinding(
      std::make_unique<ZipFileCreator>(ref_factory->CreateRef()),
      std::move(request));
}

}  // namespace

FileUtilService::FileUtilService() = default;

FileUtilService::~FileUtilService() = default;

std::unique_ptr<service_manager::Service> FileUtilService::CreateService() {
  return std::make_unique<FileUtilService>();
}

void FileUtilService::OnStart() {
  ref_factory_ = std::make_unique<service_manager::ServiceContextRefFactory>(
      base::Bind(&service_manager::ServiceContext::RequestQuit,
                 base::Unretained(context())));
  registry_.AddInterface(
      base::Bind(&OnZipFileCreatorRequest, ref_factory_.get()));
}

void FileUtilService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

}  //  namespace chrome
