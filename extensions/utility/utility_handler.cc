// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/utility/utility_handler.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "content/public/utility/utility_thread.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/extension_unpacker.mojom.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_parser.mojom.h"
#include "extensions/common/update_manifest.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "extensions/utility/unpacker.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/zlib/google/zip.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_switches.h"

namespace extensions {

namespace {

class ExtensionUnpackerImpl : public extensions::mojom::ExtensionUnpacker {
 public:
  ExtensionUnpackerImpl() = default;
  ~ExtensionUnpackerImpl() override = default;

  static void Create(extensions::mojom::ExtensionUnpackerRequest request) {
    mojo::MakeStrongBinding(base::MakeUnique<ExtensionUnpackerImpl>(),
                            std::move(request));
  }

 private:
  // extensions::mojom::ExtensionUnpacker:
  void Unzip(const base::FilePath& file,
             const base::FilePath& path,
             const UnzipCallback& callback) override {
    std::unique_ptr<base::DictionaryValue> manifest;
    if (UnzipFileManifestIntoPath(file, path, &manifest)) {
      callback.Run(UnzipFileIntoPath(file, path, std::move(manifest)));
    } else {
      callback.Run(false);
    }
  }

  void Unpack(const base::FilePath& path,
              const std::string& extension_id,
              Manifest::Location location,
              int32_t creation_flags,
              const UnpackCallback& callback) override {
    CHECK_GT(location, Manifest::INVALID_LOCATION);
    CHECK_LT(location, Manifest::NUM_LOCATIONS);
    DCHECK(ExtensionsClient::Get());

    content::UtilityThread::Get()->EnsureBlinkInitialized();

    Unpacker unpacker(path.DirName(), path, extension_id, location,
                      creation_flags);
    if (unpacker.Run()) {
      callback.Run(base::string16(), unpacker.TakeParsedManifest());
    } else {
      callback.Run(unpacker.error_message(), nullptr);
    }
  }

  static bool UnzipFileManifestIntoPath(
      const base::FilePath& file,
      const base::FilePath& path,
      std::unique_ptr<base::DictionaryValue>* manifest) {
    if (zip::UnzipWithFilterCallback(
            file, path, base::Bind(&Unpacker::IsManifestFile), false)) {
      std::string error;
      *manifest = Unpacker::ReadManifest(path, &error);
      return error.empty() && manifest->get();
    }

    return false;
  }

  static bool UnzipFileIntoPath(
      const base::FilePath& file,
      const base::FilePath& path,
      std::unique_ptr<base::DictionaryValue> manifest) {
    Manifest internal(Manifest::INTERNAL, std::move(manifest));
    // TODO(crbug.com/645263): This silently ignores blocked file types.
    //                         Add install warnings.
    return zip::UnzipWithFilterCallback(
        file, path,
        base::Bind(&Unpacker::ShouldExtractFile, internal.is_theme()),
        true /* log_skipped_files */);
  }

  DISALLOW_COPY_AND_ASSIGN(ExtensionUnpackerImpl);
};

class ManifestParserImpl : public extensions::mojom::ManifestParser {
 public:
  ManifestParserImpl() = default;
  ~ManifestParserImpl() override = default;

  static void Create(extensions::mojom::ManifestParserRequest request) {
    mojo::MakeStrongBinding(base::MakeUnique<ManifestParserImpl>(),
                            std::move(request));
  }

 private:
  void Parse(const std::string& xml, const ParseCallback& callback) override {
    UpdateManifest manifest;
    if (manifest.Parse(xml)) {
      callback.Run(manifest.results());
    } else {
      LOG(WARNING) << "Error parsing update manifest:\n" << manifest.errors();
      callback.Run(base::nullopt);
    }
  }

  DISALLOW_COPY_AND_ASSIGN(ManifestParserImpl);
};

}  // namespace

namespace utility_handler {

void UtilityThreadStarted() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::string lang = command_line->GetSwitchValueASCII(switches::kLang);
  if (!lang.empty())
    extension_l10n_util::SetProcessLocale(lang);
}

void ExposeInterfacesToBrowser(service_manager::BinderRegistry* registry,
                               bool running_elevated) {
  // If our process runs with elevated privileges, only add elevated Mojo
  // interfaces to the interface registry.
  if (running_elevated)
    return;

  registry->AddInterface(base::Bind(&ExtensionUnpackerImpl::Create),
                         base::ThreadTaskRunnerHandle::Get());
  registry->AddInterface(base::Bind(&ManifestParserImpl::Create),
                         base::ThreadTaskRunnerHandle::Get());
}

}  // namespace utility_handler

}  // namespace extensions
