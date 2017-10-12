// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_interface_proxy.h"

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/service_manager_connection.h"
#include "media/mojo/interfaces/constants.mojom.h"
#include "media/mojo/services/media_interface_provider.h"
#include "services/service_manager/public/cpp/connector.h"

#if BUILDFLAG(ENABLE_MOJO_CDM)
#include "content/public/browser/browser_context.h"
#include "content/public/browser/provision_fetcher_impl.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "net/url_request/url_request_context_getter.h"
#endif

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "base/guid.h"
#include "content/browser/media/cdm_storage_impl.h"
#include "content/public/browser/cdm_registry.h"
#include "content/public/common/cdm_info.h"
#include "media/base/key_system_names.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#if defined(OS_MACOSX)
#include "sandbox/mac/seatbelt_extension.h"
#endif  // defined(OS_MACOSX)
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

#if defined(OS_MACOSX)
#include "media/mojo/interfaces/media_service_mac.mojom.h"
#else
#include "media/mojo/interfaces/media_service.mojom.h"
#endif  // defined(OS_MACOSX)

namespace content {

#if BUILDFLAG(ENABLE_LIBRARY_CDMS) && defined(OS_MACOSX)

namespace {

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
// TODO(xhwang): Move this to a common place.
const base::FilePath::CharType kSignatureFileExtension[] =
    FILE_PATH_LITERAL(".sig");

// Returns the signature file path given the |file_path|. This function should
// only be used when the signature file and the file are located in the same
// directory, which is the case for the CDM and CDM adapter.
base::FilePath GetSigFilePath(const base::FilePath& file_path) {
  return file_path.AddExtension(kSignatureFileExtension);
}
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)

class SeatbeltExtensionTokenProviderImpl
    : public media::mojom::SeatbeltExtensionTokenProvider {
 public:
  explicit SeatbeltExtensionTokenProviderImpl(const base::FilePath& cdm_path)
      : cdm_path_(cdm_path) {}
  void GetTokens(GetTokensCallback callback) final {
    std::vector<sandbox::SeatbeltExtensionToken> tokens;

    // Allow the CDM to be loaded in the CDM service process.
    auto cdm_token = sandbox::SeatbeltExtension::Issue(
        sandbox::SeatbeltExtension::FILE_READ, cdm_path_.value());
    if (cdm_token) {
      tokens.push_back(std::move(*cdm_token));
    } else {
      std::move(callback).Run({});
      return;
    }

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
    // If CDM host verification is enabled, also allow to open the CDM signature
    // file.
    auto cdm_sig_token =
        sandbox::SeatbeltExtension::Issue(sandbox::SeatbeltExtension::FILE_READ,
                                          GetSigFilePath(cdm_path_).value());
    if (cdm_sig_token) {
      tokens.push_back(std::move(*cdm_sig_token));
    } else {
      std::move(callback).Run({});
      return;
    }
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)

    std::move(callback).Run(std::move(tokens));
  }

 private:
  base::FilePath cdm_path_;

  DISALLOW_COPY_AND_ASSIGN(SeatbeltExtensionTokenProviderImpl);
};

}  // namespace

#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS) && defined(OS_MACOSX)

MediaInterfaceProxy::MediaInterfaceProxy(
    RenderFrameHost* render_frame_host,
    media::mojom::InterfaceFactoryRequest request,
    const base::Closure& error_handler)
    : render_frame_host_(render_frame_host),
      binding_(this, std::move(request)) {
  DVLOG(1) << __func__;
  DCHECK(render_frame_host_);
  DCHECK(!error_handler.is_null());

  binding_.set_connection_error_handler(error_handler);

  // |interface_factory_ptr_| and |cdm_interface_factory_map_| will be lazily
  // connected in GetMediaInterfaceFactory() and GetCdmInterfaceFactory().
}

MediaInterfaceProxy::~MediaInterfaceProxy() {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
}

void MediaInterfaceProxy::CreateAudioDecoder(
    media::mojom::AudioDecoderRequest request) {
  DCHECK(thread_checker_.CalledOnValidThread());
  InterfaceFactory* factory = GetMediaInterfaceFactory();
  if (factory)
    factory->CreateAudioDecoder(std::move(request));
}

void MediaInterfaceProxy::CreateVideoDecoder(
    media::mojom::VideoDecoderRequest request) {
  DCHECK(thread_checker_.CalledOnValidThread());
  InterfaceFactory* factory = GetMediaInterfaceFactory();
  if (factory)
    factory->CreateVideoDecoder(std::move(request));
}

void MediaInterfaceProxy::CreateRenderer(
    const std::string& audio_device_id,
    media::mojom::RendererRequest request) {
  DCHECK(thread_checker_.CalledOnValidThread());
  InterfaceFactory* factory = GetMediaInterfaceFactory();
  if (factory)
    factory->CreateRenderer(audio_device_id, std::move(request));
}

void MediaInterfaceProxy::CreateCdm(
    const std::string& key_system,
    media::mojom::ContentDecryptionModuleRequest request) {
  DCHECK(thread_checker_.CalledOnValidThread());

  InterfaceFactory* factory =
#if !BUILDFLAG(ENABLE_STANDALONE_CDM_SERVICE)
      GetMediaInterfaceFactory();
#else
      GetCdmInterfaceFactory(key_system);
#endif

  if (factory)
    factory->CreateCdm(key_system, std::move(request));
}

service_manager::mojom::InterfaceProviderPtr
MediaInterfaceProxy::GetFrameServices(const std::string& cdm_file_system_id) {
  // Register frame services.
  service_manager::mojom::InterfaceProviderPtr interfaces;

  // TODO(xhwang): Replace this InterfaceProvider with a dedicated media host
  // interface. See http://crbug.com/660573
  auto provider = std::make_unique<media::MediaInterfaceProvider>(
      mojo::MakeRequest(&interfaces));

#if BUILDFLAG(ENABLE_MOJO_CDM)
  // TODO(slan): Wrap these into a RenderFrame specific ProvisionFetcher impl.
  net::URLRequestContextGetter* context_getter =
      BrowserContext::GetDefaultStoragePartition(
          render_frame_host_->GetProcess()->GetBrowserContext())
          ->GetURLRequestContext();
  provider->registry()->AddInterface(base::Bind(
      &ProvisionFetcherImpl::Create, base::RetainedRef(context_getter)));

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  DCHECK(!cdm_file_system_id.empty());
  provider->registry()->AddInterface(base::Bind(
      &CdmStorageImpl::Create, render_frame_host_, cdm_file_system_id));
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)
#endif  // BUILDFLAG(ENABLE_MOJO_CDM)

  GetContentClient()->browser()->ExposeInterfacesToMediaService(
      provider->registry(), render_frame_host_);

  media_registries_.push_back(std::move(provider));

  return interfaces;
}

media::mojom::InterfaceFactory*
MediaInterfaceProxy::GetMediaInterfaceFactory() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!interface_factory_ptr_)
    ConnectToMediaService();

  return interface_factory_ptr_.get();
}

void MediaInterfaceProxy::ConnectToMediaService() {
  DVLOG(1) << __func__;
  DCHECK(!interface_factory_ptr_);

  media::mojom::MediaServicePtr media_service;

  // TODO(slan): Use the BrowserContext Connector instead. See crbug.com/638950.
  service_manager::Connector* connector =
      ServiceManagerConnection::GetForProcess()->GetConnector();
  connector->BindInterface(media::mojom::kMediaServiceName, &media_service);

  media_service->CreateInterfaceFactory(MakeRequest(&interface_factory_ptr_),
                                        GetFrameServices(std::string()));

  interface_factory_ptr_.set_connection_error_handler(
      base::BindOnce(&MediaInterfaceProxy::OnMediaServiceConnectionError,
                     base::Unretained(this)));
}

void MediaInterfaceProxy::OnMediaServiceConnectionError() {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  interface_factory_ptr_.reset();
}

#if BUILDFLAG(ENABLE_STANDALONE_CDM_SERVICE)

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
// Returns CdmInfo registered for |key_system|. Returns null if no CdmInfo is
// registered for |key_system|, or if the CdmInfo registered is invalid.
static std::unique_ptr<CdmInfo> GetCdmInfoForKeySystem(
    const std::string& key_system) {
  DVLOG(2) << __func__ << ": key_system = " << key_system;
  for (const auto& cdm : CdmRegistry::GetInstance()->GetAllRegisteredCdms()) {
    if (cdm.supported_key_system == key_system ||
        (cdm.supports_sub_key_systems &&
         media::IsChildKeySystemOf(key_system, cdm.supported_key_system))) {
      return std::make_unique<CdmInfo>(cdm);
    }
  }

  return nullptr;
}
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

media::mojom::InterfaceFactory* MediaInterfaceProxy::GetCdmInterfaceFactory(
    const std::string& key_system) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Set the CDM GUID to be the default user ID used when connecting to mojo
  // services. This is needed when ENABLE_STANDALONE_CDM_SERVICE is true but
  // ENABLE_LIBRARY_CDMS is false.
  std::string cdm_guid = service_manager::mojom::kInheritUserID;

  base::FilePath cdm_path;
  std::string cdm_file_system_id;

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  std::unique_ptr<CdmInfo> cdm_info = GetCdmInfoForKeySystem(key_system);
  if (!cdm_info) {
    NOTREACHED() << "No valid CdmInfo for " << key_system;
    return nullptr;
  }
  if (cdm_info->path.empty()) {
    NOTREACHED() << "CDM path for " << key_system << " is empty.";
    return nullptr;
  }
  if (!base::IsValidGUID(cdm_info->guid)) {
    NOTREACHED() << "Invalid CDM GUID " << cdm_info->guid;
    return nullptr;
  }
  if (!CdmStorageImpl::IsValidCdmFileSystemId(cdm_info->file_system_id)) {
    NOTREACHED() << "Invalid file system ID " << cdm_info->file_system_id;
    return nullptr;
  }
  cdm_guid = cdm_info->guid;
  cdm_path = cdm_info->path;
  cdm_file_system_id = cdm_info->file_system_id;
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

  auto found = cdm_interface_factory_map_.find(cdm_guid);
  if (found != cdm_interface_factory_map_.end())
    return found->second.get();

  return ConnectToCdmService(cdm_guid, cdm_path, cdm_file_system_id);
}

media::mojom::InterfaceFactory* MediaInterfaceProxy::ConnectToCdmService(
    const std::string& cdm_guid,
    const base::FilePath& cdm_path,
    const std::string& cdm_file_system_id) {
  DVLOG(1) << __func__ << ": cdm_guid = " << cdm_guid;

  DCHECK(!cdm_interface_factory_map_.count(cdm_guid));
  service_manager::Identity identity(media::mojom::kCdmServiceName, cdm_guid);

  // TODO(slan): Use the BrowserContext Connector instead. See crbug.com/638950.
  service_manager::Connector* connector =
      ServiceManagerConnection::GetForProcess()->GetConnector();

  media::mojom::MediaServicePtr media_service;
  connector->BindInterface(identity, &media_service);

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#if defined(OS_MACOSX)
  // LoadCdm() should always be called before CreateInterfaceFactory().
  media::mojom::SeatbeltExtensionTokenProviderPtr token_provider_ptr;
  mojo::MakeStrongBinding(
      std::make_unique<SeatbeltExtensionTokenProviderImpl>(cdm_path),
      mojo::MakeRequest(&token_provider_ptr));

  media_service->LoadCdm(cdm_path, std::move(token_provider_ptr));
#else
  media_service->LoadCdm(cdm_path);
#endif  // defined(OS_MACOSX)
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

  InterfaceFactoryPtr interface_factory_ptr;
  media_service->CreateInterfaceFactory(MakeRequest(&interface_factory_ptr),
                                        GetFrameServices(cdm_file_system_id));
  interface_factory_ptr.set_connection_error_handler(
      base::BindOnce(&MediaInterfaceProxy::OnCdmServiceConnectionError,
                     base::Unretained(this), cdm_guid));

  InterfaceFactory* cdm_interface_factory = interface_factory_ptr.get();
  cdm_interface_factory_map_.emplace(cdm_guid,
                                     std::move(interface_factory_ptr));
  return cdm_interface_factory;
}

void MediaInterfaceProxy::OnCdmServiceConnectionError(
    const std::string& cdm_guid) {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  DCHECK(cdm_interface_factory_map_.count(cdm_guid));
  cdm_interface_factory_map_.erase(cdm_guid);
}

#endif  // BUILDFLAG(ENABLE_STANDALONE_CDM_SERVICE)

}  // namespace content
