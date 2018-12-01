// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/importer/profile_import_service.h"

#include "build/build_config.h"
#include "chrome/utility/importer/profile_import_impl.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

#if defined(OS_MACOSX)
#include <stdlib.h>

#include "chrome/common/importer/firefox_importer_utils.h"
#endif

namespace {

void OnProfileImportRequest(service_manager::ServiceKeepalive* keepalive,
                            chrome::mojom::ProfileImportRequest request) {
  mojo::MakeStrongBinding(
      std::make_unique<ProfileImportImpl>(keepalive->CreateRef()),
      std::move(request));
}

}  // namespace

ProfileImportService::ProfileImportService(
    service_manager::mojom::ServiceRequest request)
    : service_binding_(this, std::move(request)),
      service_keepalive_(&service_binding_, base::TimeDelta()) {}

ProfileImportService::~ProfileImportService() = default;

void ProfileImportService::OnStart() {
  registry_.AddInterface(
      base::BindRepeating(&OnProfileImportRequest, &service_keepalive_));

#if defined(OS_MACOSX)
  std::string dylib_path = GetFirefoxDylibPath().value();
  if (!dylib_path.empty())
    ::setenv("DYLD_FALLBACK_LIBRARY_PATH", dylib_path.c_str(),
             1 /* overwrite */);
#endif
}

void ProfileImportService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}
