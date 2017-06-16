// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_EXTENSIONS_EXTENSIONS_HANDLER_H_
#define CHROME_UTILITY_EXTENSIONS_EXTENSIONS_HANDLER_H_

#include <stdint.h>

#include "base/base64.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/common/media_galleries/picasa_types.h"
#include "chrome/utility/utility_message_handler.h"
#include "extensions/features/features.h"
#include "extensions/utility/utility_handler.h"
#include "services/service_manager/public/cpp/binder_registry.h"

#if !BUILDFLAG(ENABLE_EXTENSIONS)
#error "Extensions must be enabled"
#endif

namespace extensions {

// Dispatches IPCs for Chrome extensions utility messages.
// Note: these IPC are deprecated so there is no need to convert
// them to mojo. https://crbug.com/680928
class ExtensionsHandler : public UtilityMessageHandler {
 public:
  ExtensionsHandler();
  ~ExtensionsHandler() override;

  static void PreSandboxStartup();

  static void ExposeInterfacesToBrowser(
      service_manager::BinderRegistry* registry,
      bool running_elevated);

  // UtilityMessageHandler:
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  // IPC message handlers.
#if defined(OS_WIN)
  void OnParseITunesPrefXml(const std::string& itunes_xml_data);
#endif  // defined(OS_WIN)

#if defined(OS_WIN) || defined(OS_MACOSX)
  void OnParseITunesLibraryXmlFile(
      const IPC::PlatformFileForTransit& itunes_library_file);

  void OnParsePicasaPMPDatabase(
      const picasa::AlbumTableFilesForTransit& album_table_files);

  void OnIndexPicasaAlbumsContents(
      const picasa::AlbumUIDSet& album_uids,
      const std::vector<picasa::FolderINIContents>& folders_inis);
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

  DISALLOW_COPY_AND_ASSIGN(ExtensionsHandler);
};

}  // namespace extensions

#endif  // CHROME_UTILITY_EXTENSIONS_EXTENSIONS_HANDLER_H_
