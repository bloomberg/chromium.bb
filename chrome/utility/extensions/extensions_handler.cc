// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/extensions/extensions_handler.h"

#include <utility>

#include "base/files/file_path.h"
#include "chrome/common/extensions/chrome_extensions_client.h"
#include "chrome/common/extensions/removable_storage_writer.mojom.h"
#include "chrome/common/media_galleries/metadata_types.h"
#include "chrome/utility/image_writer/image_writer_handler.h"
#include "content/public/utility/utility_thread.h"
#include "media/base/media.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace {

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
#endif
    return;
  }

#if !defined(OS_WIN)
  registry->AddInterface(base::Bind(&RemovableStorageWriterImpl::Create),
                         base::ThreadTaskRunnerHandle::Get());
#endif
}

}  // namespace extensions
