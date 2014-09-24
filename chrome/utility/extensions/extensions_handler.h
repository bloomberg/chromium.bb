// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_EXTENSIONS_EXTENSIONS_HANDLER_H_
#define CHROME_UTILITY_EXTENSIONS_EXTENSIONS_HANDLER_H_

#include "base/base64.h"
#include "base/compiler_specific.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "chrome/common/media_galleries/picasa_types.h"
#include "chrome/utility/utility_message_handler.h"

#if !defined(ENABLE_EXTENSIONS)
#error "Extensions must be enabled"
#endif

namespace metadata {
class MediaMetadataParser;
}

namespace extensions {

// Dispatches IPCs for Chrome extensions utility messages.
class ExtensionsHandler : public UtilityMessageHandler {
 public:
  ExtensionsHandler();
  virtual ~ExtensionsHandler();

  static void PreSandboxStartup();
  static void UtilityThreadStarted();

  // IPC::Listener:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  // IPC message handlers.
  void OnUnpackExtension(const base::FilePath& extension_path,
                         const std::string& extension_id,
                         int location, int creation_flags);
  void OnUnzipToDir(const base::FilePath& zip_path, const base::FilePath& dir);
  void OnParseUpdateManifest(const std::string& xml);
  void OnDecodeImageBase64(const std::string& encoded_data);
  void OnParseJSON(const std::string& json);
  void OnCheckMediaFile(int64 milliseconds_of_decoding,
                        const IPC::PlatformFileForTransit& media_file);

#if defined(OS_WIN)
  void OnParseITunesPrefXml(const std::string& itunes_xml_data);
#endif  // defined(OS_WIN)

#if defined(OS_MACOSX)
  void OnParseIPhotoLibraryXmlFile(
      const IPC::PlatformFileForTransit& iphoto_library_file);
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN) || defined(OS_MACOSX)
  void OnParseITunesLibraryXmlFile(
      const IPC::PlatformFileForTransit& itunes_library_file);

  void OnParsePicasaPMPDatabase(
      const picasa::AlbumTableFilesForTransit& album_table_files);

  void OnIndexPicasaAlbumsContents(
      const picasa::AlbumUIDSet& album_uids,
      const std::vector<picasa::FolderINIContents>& folders_inis);
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

#if defined(OS_WIN)
  void OnGetWiFiCredentials(const std::string& network_guid);
#endif  // defined(OS_WIN)

  DISALLOW_COPY_AND_ASSIGN(ExtensionsHandler);
};

}  // namespace extensions

#endif  // CHROME_UTILITY_EXTENSIONS_EXTENSIONS_HANDLER_H_
