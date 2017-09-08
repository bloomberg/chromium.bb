// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/extensions/extensions_handler.h"

#include <utility>

#include "base/files/file_path.h"
#include "chrome/common/extensions/chrome_extensions_client.h"
#include "chrome/common/extensions/media_parser.mojom.h"
#include "chrome/common/extensions/removable_storage_writer.mojom.h"
#include "chrome/common/media_galleries/metadata_types.h"
#include "chrome/utility/image_writer/image_writer_handler.h"
#include "chrome/utility/media_galleries/ipc_data_source.h"
#include "chrome/utility/media_galleries/media_metadata_parser.h"
#include "content/public/utility/utility_thread.h"
#include "media/base/media.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

#if !defined(MEDIA_DISABLE_FFMPEG)
#include "media/filters/media_file_checker.h"
#endif

#if defined(OS_WIN)
#include "chrome/common/extensions/wifi_credentials_getter.mojom.h"
#include "components/wifi/wifi_service.h"
#endif  // defined(OS_WIN)

namespace {

class MediaParserImpl : public extensions::mojom::MediaParser {
 public:
  MediaParserImpl() = default;
  ~MediaParserImpl() override = default;

  static void Create(extensions::mojom::MediaParserRequest request) {
    mojo::MakeStrongBinding(base::MakeUnique<MediaParserImpl>(),
                            std::move(request));
  }

 private:
  // extensions::mojom::MediaParser:
  void ParseMediaMetadata(
      const std::string& mime_type,
      int64_t total_size,
      bool get_attached_images,
      extensions::mojom::MediaDataSourcePtr media_data_source,
      ParseMediaMetadataCallback callback) override {
    auto source = base::MakeUnique<metadata::IPCDataSource>(
        std::move(media_data_source), total_size);
    metadata::MediaMetadataParser* parser = new metadata::MediaMetadataParser(
        std::move(source), mime_type, get_attached_images);
    parser->Start(base::Bind(&MediaParserImpl::ParseMediaMetadataDone,
                             base::Passed(&callback), base::Owned(parser)));
  }

  static void ParseMediaMetadataDone(
      ParseMediaMetadataCallback callback,
      metadata::MediaMetadataParser* /* parser */,
      const extensions::api::media_galleries::MediaMetadata& metadata,
      const std::vector<metadata::AttachedImage>& attached_images) {
    std::move(callback).Run(true, metadata.ToValue(), attached_images);
  }

  void CheckMediaFile(base::TimeDelta decode_time,
                      base::File file,
                      CheckMediaFileCallback callback) override {
#if !defined(MEDIA_DISABLE_FFMPEG)
    media::MediaFileChecker checker(std::move(file));
    std::move(callback).Run(checker.Start(decode_time));
#else
    std::move(callback).Run(false);
#endif
  }

  DISALLOW_COPY_AND_ASSIGN(MediaParserImpl);
};

class RemovableStorageWriterImpl
    : public extensions::mojom::RemovableStorageWriter {
 public:
  RemovableStorageWriterImpl() = default;
  ~RemovableStorageWriterImpl() override = default;

  static void Create(extensions::mojom::RemovableStorageWriterRequest request) {
    mojo::MakeStrongBinding(base::MakeUnique<RemovableStorageWriterImpl>(),
                            std::move(request));
  }

 private:
  void Write(
      const base::FilePath& source,
      const base::FilePath& target,
      extensions::mojom::RemovableStorageWriterClientPtr client) override {
    writer_.Write(source, target, std::move(client));
  }

  void Verify(
      const base::FilePath& source,
      const base::FilePath& target,
      extensions::mojom::RemovableStorageWriterClientPtr client) override {
    writer_.Verify(source, target, std::move(client));
  }

  image_writer::ImageWriterHandler writer_;

  DISALLOW_COPY_AND_ASSIGN(RemovableStorageWriterImpl);
};

#if defined(OS_WIN)
class WiFiCredentialsGetterImpl
    : public extensions::mojom::WiFiCredentialsGetter {
 public:
  WiFiCredentialsGetterImpl() = default;
  ~WiFiCredentialsGetterImpl() override = default;

  static void Create(extensions::mojom::WiFiCredentialsGetterRequest request) {
    mojo::MakeStrongBinding(base::MakeUnique<WiFiCredentialsGetterImpl>(),
                            std::move(request));
  }

 private:
  // extensions::mojom::WiFiCredentialsGetter:
  void GetWiFiCredentials(const std::string& ssid,
                          GetWiFiCredentialsCallback callback) override {
    if (ssid == kWiFiTestNetwork) {
      // test-mode: return the ssid in key_data.
      std::move(callback).Run(true, ssid);
      return;
    }

    std::unique_ptr<wifi::WiFiService> wifi_service(
        wifi::WiFiService::Create());
    wifi_service->Initialize(nullptr);

    std::string key_data;
    std::string error;
    wifi_service->GetKeyFromSystem(ssid, &key_data, &error);

    const bool success = error.empty();
    if (!success)
      key_data.clear();

    std::move(callback).Run(success, key_data);
  }

  DISALLOW_COPY_AND_ASSIGN(WiFiCredentialsGetterImpl);
};
#endif  // defined(OS_WIN)

}  // namespace

namespace extensions {

// static
void InitExtensionsClient() {
  ExtensionsClient::Set(ChromeExtensionsClient::GetInstance());
}

// static
void PreSandboxStartup() {
  media::InitializeMediaLibrary();  // Used for media file validation.
}

// static
void ExposeInterfacesToBrowser(service_manager::BinderRegistry* registry,
                               bool running_elevated) {
  // If our process runs with elevated privileges, only add elevated Mojo
  // interfaces to the interface registry.
  if (running_elevated) {
#if defined(OS_WIN)
    registry->AddInterface(base::Bind(&RemovableStorageWriterImpl::Create),
                           base::ThreadTaskRunnerHandle::Get());
    registry->AddInterface(base::Bind(&WiFiCredentialsGetterImpl::Create),
                           base::ThreadTaskRunnerHandle::Get());
#endif
    return;
  }

  registry->AddInterface(base::Bind(&MediaParserImpl::Create),
                         base::ThreadTaskRunnerHandle::Get());
#if !defined(OS_WIN)
  registry->AddInterface(base::Bind(&RemovableStorageWriterImpl::Create),
                         base::ThreadTaskRunnerHandle::Get());
#endif
}

}  // namespace extensions
