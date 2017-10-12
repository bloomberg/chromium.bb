// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/media_service.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "media/base/media_log.h"
#include "media/media_features.h"
#include "media/mojo/services/interface_factory_impl.h"
#include "media/mojo/services/mojo_media_client.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/connector.h"

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "media/cdm/cdm_module.h"
#if defined(OS_MACOSX)
#include "sandbox/mac/seatbelt_extension.h"
#endif  // defined(OS_MACOSX)
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

namespace media {

MediaService::MediaService(std::unique_ptr<MojoMediaClient> mojo_media_client)
    : mojo_media_client_(std::move(mojo_media_client)) {
  DCHECK(mojo_media_client_);
  registry_.AddInterface<mojom::MediaService>(
      base::Bind(&MediaService::Create, base::Unretained(this)));
}

MediaService::~MediaService() {}

void MediaService::OnStart() {
  DVLOG(1) << __func__;

  ref_factory_.reset(new service_manager::ServiceContextRefFactory(
      base::Bind(&service_manager::ServiceContext::RequestQuit,
                 base::Unretained(context()))));
  mojo_media_client_->Initialize(context()->connector(), ref_factory_.get());
}

void MediaService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  DVLOG(1) << __func__ << ": interface_name = " << interface_name;

  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

bool MediaService::OnServiceManagerConnectionLost() {
  interface_factory_bindings_.CloseAllBindings();
  mojo_media_client_.reset();
  return true;
}

void MediaService::Create(mojom::MediaServiceRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

#if defined(OS_MACOSX)
void MediaService::LoadCdm(
    const base::FilePath& cdm_path,
    mojom::SeatbeltExtensionTokenProviderPtr token_provider) {
#else
void MediaService::LoadCdm(const base::FilePath& cdm_path) {
#endif  // defined(OS_MACOSX)
  DVLOG(1) << __func__ << ": cdm_path = " << cdm_path.value();

  // Ignore request if service has already stopped.
  if (!mojo_media_client_)
    return;

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  CdmModule* instance = CdmModule::GetInstance();
  if (instance->was_initialize_called()) {
    DCHECK_EQ(cdm_path, instance->GetCdmPath());
    return;
  }

#if defined(OS_MACOSX)
  std::vector<sandbox::SeatbeltExtensionToken> tokens;
  CHECK(token_provider->GetTokens(&tokens));

  std::vector<std::unique_ptr<sandbox::SeatbeltExtension>> extensions;

  for (auto&& token : tokens) {
    DVLOG(3) << "token: " << token.token();
    auto extension = sandbox::SeatbeltExtension::FromToken(std::move(token));
    if (!extension->Consume()) {
      DVLOG(1) << "Failed to comsume sandbox seatbelt extension. This could "
                  "happen if --no-sandbox is specified.";
    }
    extensions.push_back(std::move(extension));
  }
#endif  // defined(OS_MACOSX)

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
  std::vector<CdmHostFilePath> cdm_host_file_paths;
  mojo_media_client_->AddCdmHostFilePaths(&cdm_host_file_paths);
  if (!instance->Initialize(cdm_path, cdm_host_file_paths))
    return;
#else
  if (!instance->Initialize(cdm_path))
    return;
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)

  // This may trigger the sandbox to be sealed.
  mojo_media_client_->EnsureSandboxed();

#if defined(OS_MACOSX)
  for (auto&& extension : extensions)
    extension->Revoke();
#endif  // defined(OS_MACOSX)

  // Always called within the sandbox.
  instance->InitializeCdmModule();
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)
}

void MediaService::CreateInterfaceFactory(
    mojom::InterfaceFactoryRequest request,
    service_manager::mojom::InterfaceProviderPtr host_interfaces) {
  // Ignore request if service has already stopped.
  if (!mojo_media_client_)
    return;

  interface_factory_bindings_.AddBinding(
      base::MakeUnique<InterfaceFactoryImpl>(
          std::move(host_interfaces), &media_log_, ref_factory_->CreateRef(),
          mojo_media_client_.get()),
      std::move(request));
}

}  // namespace media
