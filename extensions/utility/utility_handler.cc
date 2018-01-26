// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/utility/utility_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/location.h"
#include "base/task_scheduler/post_task.h"
#include "content/public/utility/utility_thread.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/extension_unpacker.mojom.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/features/feature_session_type.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "third_party/zlib/google/zip.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_switches.h"

namespace extensions {

namespace {

constexpr const base::FilePath::CharType* kAllowedThemeFiletypes[] = {
    FILE_PATH_LITERAL(".bmp"),  FILE_PATH_LITERAL(".gif"),
    FILE_PATH_LITERAL(".jpeg"), FILE_PATH_LITERAL(".jpg"),
    FILE_PATH_LITERAL(".json"), FILE_PATH_LITERAL(".png"),
    FILE_PATH_LITERAL(".webp")};

std::unique_ptr<base::DictionaryValue> ReadManifest(
    const base::FilePath& extension_dir,
    std::string* error) {
  DCHECK(error);
  base::FilePath manifest_path = extension_dir.Append(kManifestFilename);
  if (!base::PathExists(manifest_path)) {
    *error = manifest_errors::kInvalidManifest;
    return nullptr;
  }

  JSONFileValueDeserializer deserializer(manifest_path);
  std::unique_ptr<base::Value> root = deserializer.Deserialize(NULL, error);
  if (!root) {
    return nullptr;
  }

  if (!root->is_dict()) {
    *error = manifest_errors::kInvalidManifest;
    return nullptr;
  }

  return base::DictionaryValue::From(std::move(root));
}

class ExtensionUnpackerImpl : public extensions::mojom::ExtensionUnpacker {
 public:
  ExtensionUnpackerImpl() = default;
  ~ExtensionUnpackerImpl() override = default;

  static void Create(extensions::mojom::ExtensionUnpackerRequest request) {
    mojo::MakeStrongBinding(std::make_unique<ExtensionUnpackerImpl>(),
                            std::move(request));
  }

 private:
  // extensions::mojom::ExtensionUnpacker:
  void Unzip(const base::FilePath& file,
             const base::FilePath& path,
             UnzipCallback callback) override {
    // Move unzip operation to background thread to avoid blocking the main
    // utility thread for extended amont of time. For example, this prevents
    // extension unzipping block receipt of the connection complete
    // notification for the utility process channel to the browser process,
    // which could cause the utility process to terminate itself due to browser
    // process being considered unreachable.
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE,
        {base::TaskPriority::USER_BLOCKING, base::MayBlock(),
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
        base::BindOnce(&ExtensionUnpackerImpl::UnzipOnBackgroundTaskRunner,
                       file, path),
        std::move(callback));
  }

  static bool UnzipFileManifestIntoPath(
      const base::FilePath& file,
      const base::FilePath& path,
      std::unique_ptr<base::DictionaryValue>* manifest) {
    if (zip::UnzipWithFilterCallback(
            file, path, base::BindRepeating(&utility_handler::IsManifestFile),
            false)) {
      std::string error;
      *manifest = ReadManifest(path, &error);
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
        base::BindRepeating(&utility_handler::ShouldExtractFile,
                            internal.is_theme()),
        true /* log_skipped_files */);
  }

  // Unzips the extension from |file| to |path|.
  // Returns whether the unzip operation succeeded.
  static bool UnzipOnBackgroundTaskRunner(const base::FilePath& file,
                                          const base::FilePath& path) {
    std::unique_ptr<base::DictionaryValue> manifest;
    if (!UnzipFileManifestIntoPath(file, path, &manifest))
      return false;

    return UnzipFileIntoPath(file, path, std::move(manifest));
  }

  DISALLOW_COPY_AND_ASSIGN(ExtensionUnpackerImpl);
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
}

bool ShouldExtractFile(bool is_theme, const base::FilePath& file_path) {
  if (is_theme) {
    const base::FilePath::StringType extension =
        base::ToLowerASCII(file_path.FinalExtension());
    // Allow filenames with no extension.
    if (extension.empty())
      return true;
    return base::ContainsValue(kAllowedThemeFiletypes, extension);
  }
  return !base::FilePath::CompareEqualIgnoreCase(file_path.FinalExtension(),
                                                 FILE_PATH_LITERAL(".exe"));
}

bool IsManifestFile(const base::FilePath& file_path) {
  CHECK(!file_path.IsAbsolute());
  return base::FilePath::CompareEqualIgnoreCase(file_path.value(),
                                                kManifestFilename);
}

}  // namespace utility_handler

}  // namespace extensions
