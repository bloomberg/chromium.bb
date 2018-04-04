// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/cdm_service.h"

#include "base/logging.h"
#include "media/base/cdm_factory.h"
#include "media/cdm/cdm_module.h"
#include "media/media_buildflags.h"
#include "media/mojo/services/mojo_cdm_service.h"
#include "media/mojo/services/mojo_cdm_service_context.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service_context.h"

#if defined(OS_MACOSX)
#include <vector>
#include "sandbox/mac/seatbelt_extension.h"
#endif  // defined(OS_MACOSX)

namespace media {

namespace {

constexpr base::TimeDelta kServiceContextRefReleaseDelay =
    base::TimeDelta::FromSeconds(5);

void DeleteServiceContextRef(service_manager::ServiceContextRef* ref) {
  delete ref;
}

// Starting a new process and loading the library CDM could be expensive. This
// class helps delay the release of service_manager::ServiceContextRef by
// |kServiceContextRefReleaseDelay|, which will ultimately delay CdmService
// destruction by the same delay as well. This helps reduce the chance of
// destroying the CdmService and immediately creates it (in another process) in
// cases like navigation, which could cause long service connection delays.
class DelayedReleaseServiceContextRef {
 public:
  explicit DelayedReleaseServiceContextRef(
      std::unique_ptr<service_manager::ServiceContextRef> ref)
      : ref_(std::move(ref)),
        task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

  ~DelayedReleaseServiceContextRef() {
    service_manager::ServiceContextRef* ref_ptr = ref_.release();
    if (!task_runner_->PostNonNestableDelayedTask(
            FROM_HERE, base::BindOnce(&DeleteServiceContextRef, ref_ptr),
            kServiceContextRefReleaseDelay)) {
      DeleteServiceContextRef(ref_ptr);
    }
  }

 private:
  std::unique_ptr<service_manager::ServiceContextRef> ref_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(DelayedReleaseServiceContextRef);
};

class CdmFactoryImpl : public mojom::CdmFactory {
 public:
  CdmFactoryImpl(
      CdmService::Client* client,
      service_manager::mojom::InterfaceProviderPtr interfaces,
      std::unique_ptr<service_manager::ServiceContextRef> connection_ref)
      : client_(client),
        interfaces_(std::move(interfaces)),
        connection_ref_(std::make_unique<DelayedReleaseServiceContextRef>(
            std::move(connection_ref))) {}

  ~CdmFactoryImpl() final {}

  // mojom::CdmFactory implementation.
  void CreateCdm(const std::string& key_system,
                 mojom::ContentDecryptionModuleRequest request) final {
    auto* cdm_factory = GetCdmFactory();
    if (!cdm_factory)
      return;

    cdm_bindings_.AddBinding(
        std::make_unique<MojoCdmService>(cdm_factory, &cdm_service_context_),
        std::move(request));
  }

 private:
  media::CdmFactory* GetCdmFactory() {
    if (!cdm_factory_) {
      cdm_factory_ = client_->CreateCdmFactory(interfaces_.get());
      DLOG_IF(ERROR, !cdm_factory_) << "CdmFactory not available.";
    }
    return cdm_factory_.get();
  }

  // Must be declared before the bindings below because the bound objects might
  // take a raw pointer of |cdm_service_context_| and assume it's always
  // available.
  MojoCdmServiceContext cdm_service_context_;

  CdmService::Client* client_;
  service_manager::mojom::InterfaceProviderPtr interfaces_;
  mojo::StrongBindingSet<mojom::ContentDecryptionModule> cdm_bindings_;
  std::unique_ptr<DelayedReleaseServiceContextRef> connection_ref_;
  std::unique_ptr<media::CdmFactory> cdm_factory_;

  DISALLOW_COPY_AND_ASSIGN(CdmFactoryImpl);
};

}  // namespace

CdmService::CdmService(std::unique_ptr<Client> client)
    : client_(std::move(client)) {
  DCHECK(client_);
  registry_.AddInterface<mojom::CdmService>(
      base::BindRepeating(&CdmService::Create, base::Unretained(this)));
}

CdmService::~CdmService() = default;

void CdmService::OnStart() {
  DVLOG(1) << __func__;

  ref_factory_.reset(new service_manager::ServiceContextRefFactory(
      context()->CreateQuitClosure()));
}

void CdmService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  DVLOG(1) << __func__ << ": interface_name = " << interface_name;

  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

bool CdmService::OnServiceManagerConnectionLost() {
  cdm_factory_bindings_.CloseAllBindings();
  client_.reset();
  return true;
}

void CdmService::Create(mojom::CdmServiceRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

#if defined(OS_MACOSX)
void CdmService::LoadCdm(
    const base::FilePath& cdm_path,
    mojom::SeatbeltExtensionTokenProviderPtr token_provider) {
#else
void CdmService::LoadCdm(const base::FilePath& cdm_path) {
#endif  // defined(OS_MACOSX)
  DVLOG(1) << __func__ << ": cdm_path = " << cdm_path.value();

  // Ignore request if service has already stopped.
  if (!client_)
    return;

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
      DVLOG(1) << "Failed to consume sandbox seatbelt extension. This could "
                  "happen if --no-sandbox is specified.";
    }
    extensions.push_back(std::move(extension));
  }
#endif  // defined(OS_MACOSX)

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
  std::vector<CdmHostFilePath> cdm_host_file_paths;
  client_->AddCdmHostFilePaths(&cdm_host_file_paths);
  if (!instance->Initialize(cdm_path, cdm_host_file_paths))
    return;
#else
  if (!instance->Initialize(cdm_path))
    return;
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)

  // This may trigger the sandbox to be sealed.
  client_->EnsureSandboxed();

#if defined(OS_MACOSX)
  for (auto&& extension : extensions)
    extension->Revoke();
#endif  // defined(OS_MACOSX)

  // Always called within the sandbox.
  instance->InitializeCdmModule();
}

void CdmService::CreateCdmFactory(
    mojom::CdmFactoryRequest request,
    service_manager::mojom::InterfaceProviderPtr host_interfaces) {
  // Ignore request if service has already stopped.
  if (!client_)
    return;

  cdm_factory_bindings_.AddBinding(
      std::make_unique<CdmFactoryImpl>(
          client_.get(), std::move(host_interfaces), ref_factory_->CreateRef()),
      std::move(request));
}

}  // namespace media
