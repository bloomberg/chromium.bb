// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/extensions/extensions_handler.h"

#include <utility>

#include "base/command_line.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "chrome/common/chrome_utility_messages.h"
#include "chrome/common/extensions/chrome_extensions_client.h"
#include "chrome/common/extensions/chrome_utility_extensions_messages.h"
#include "chrome/common/media_galleries/metadata_types.h"
#include "chrome/utility/chrome_content_utility_client.h"
#include "chrome/utility/media_galleries/ipc_data_source.h"
#include "chrome/utility/media_galleries/media_metadata_parser.h"
#include "content/public/common/content_paths.h"
#include "content/public/utility/utility_thread.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/extension_utility_messages.h"
#include "extensions/utility/unpacker.h"
#include "media/base/media.h"
#include "ui/base/ui_base_switches.h"

#if !defined(MEDIA_DISABLE_FFMPEG)
#include "media/base/media_file_checker.h"
#endif

#if defined(OS_WIN)
#include "chrome/common/extensions/api/networking_private/networking_private_crypto.h"
#include "chrome/utility/media_galleries/itunes_pref_parser_win.h"
#include "components/wifi/wifi_service.h"
#endif  // defined(OS_WIN)

#if defined(OS_WIN) || defined(OS_MACOSX)
#include "chrome/utility/media_galleries/iapps_xml_utils.h"
#include "chrome/utility/media_galleries/itunes_library_parser.h"
#include "chrome/utility/media_galleries/picasa_album_table_reader.h"
#include "chrome/utility/media_galleries/picasa_albums_indexer.h"
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

namespace extensions {

namespace {

bool Send(IPC::Message* message) {
  return content::UtilityThread::Get()->Send(message);
}

void ReleaseProcessIfNeeded() {
  content::UtilityThread::Get()->ReleaseProcessIfNeeded();
}

void FinishParseMediaMetadata(
    metadata::MediaMetadataParser* /* parser */,
    const extensions::api::media_galleries::MediaMetadata& metadata,
    const std::vector<metadata::AttachedImage>& attached_images) {
  Send(new ChromeUtilityHostMsg_ParseMediaMetadata_Finished(
      true, *metadata.ToValue(), attached_images));
  ReleaseProcessIfNeeded();
}

}  // namespace

ExtensionsHandler::ExtensionsHandler(ChromeContentUtilityClient* utility_client)
    : utility_client_(utility_client) {
  ExtensionsClient::Set(ChromeExtensionsClient::GetInstance());
}

ExtensionsHandler::~ExtensionsHandler() {
}

// static
void ExtensionsHandler::PreSandboxStartup() {
  // Initialize media libraries for media file validation.
  media::InitializeMediaLibrary();
}

bool ExtensionsHandler::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExtensionsHandler, message)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_CheckMediaFile, OnCheckMediaFile)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_ParseMediaMetadata,
                        OnParseMediaMetadata)

#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_ParseITunesPrefXml,
                        OnParseITunesPrefXml)
#endif  // defined(OS_WIN)

#if defined(OS_WIN) || defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_ParseITunesLibraryXmlFile,
                        OnParseITunesLibraryXmlFile)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_ParsePicasaPMPDatabase,
                        OnParsePicasaPMPDatabase)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_IndexPicasaAlbumsContents,
                        OnIndexPicasaAlbumsContents)
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_GetWiFiCredentials,
                        OnGetWiFiCredentials)
#endif  // defined(OS_WIN)

    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled || utility_handler_.OnMessageReceived(message);
}

void ExtensionsHandler::OnCheckMediaFile(
    int64_t milliseconds_of_decoding,
    const IPC::PlatformFileForTransit& media_file) {
#if !defined(MEDIA_DISABLE_FFMPEG)
  media::MediaFileChecker checker(
      IPC::PlatformFileForTransitToFile(media_file));
  const bool check_success = checker.Start(
      base::TimeDelta::FromMilliseconds(milliseconds_of_decoding));
  Send(new ChromeUtilityHostMsg_CheckMediaFile_Finished(check_success));
#else
  Send(new ChromeUtilityHostMsg_CheckMediaFile_Finished(false));
#endif
  ReleaseProcessIfNeeded();
}

void ExtensionsHandler::OnParseMediaMetadata(const std::string& mime_type,
                                             int64_t total_size,
                                             bool get_attached_images) {
  // Only one IPCDataSource may be created and added to the list of handlers.
  std::unique_ptr<metadata::IPCDataSource> source(
      new metadata::IPCDataSource(total_size));
  metadata::MediaMetadataParser* parser = new metadata::MediaMetadataParser(
      source.get(), mime_type, get_attached_images);
  utility_client_->AddHandler(std::move(source));
  parser->Start(base::Bind(&FinishParseMediaMetadata, base::Owned(parser)));
}

#if defined(OS_WIN)
void ExtensionsHandler::OnParseITunesPrefXml(
    const std::string& itunes_xml_data) {
  base::FilePath library_path(
      itunes::FindLibraryLocationInPrefXml(itunes_xml_data));
  Send(new ChromeUtilityHostMsg_GotITunesDirectory(library_path));
  ReleaseProcessIfNeeded();
}
#endif  // defined(OS_WIN)

#if defined(OS_WIN) || defined(OS_MACOSX)
void ExtensionsHandler::OnParseITunesLibraryXmlFile(
    const IPC::PlatformFileForTransit& itunes_library_file) {
  itunes::ITunesLibraryParser parser;
  base::File file = IPC::PlatformFileForTransitToFile(itunes_library_file);
  bool result = parser.Parse(iapps::ReadFileAsString(std::move(file)));
  Send(new ChromeUtilityHostMsg_GotITunesLibrary(result, parser.library()));
  ReleaseProcessIfNeeded();
}

void ExtensionsHandler::OnParsePicasaPMPDatabase(
    const picasa::AlbumTableFilesForTransit& album_table_files) {
  picasa::AlbumTableFiles files;
  files.indicator_file =
      IPC::PlatformFileForTransitToFile(album_table_files.indicator_file);
  files.category_file =
      IPC::PlatformFileForTransitToFile(album_table_files.category_file);
  files.date_file =
      IPC::PlatformFileForTransitToFile(album_table_files.date_file);
  files.filename_file =
      IPC::PlatformFileForTransitToFile(album_table_files.filename_file);
  files.name_file =
      IPC::PlatformFileForTransitToFile(album_table_files.name_file);
  files.token_file =
      IPC::PlatformFileForTransitToFile(album_table_files.token_file);
  files.uid_file =
      IPC::PlatformFileForTransitToFile(album_table_files.uid_file);

  picasa::PicasaAlbumTableReader reader(std::move(files));
  bool parse_success = reader.Init();
  Send(new ChromeUtilityHostMsg_ParsePicasaPMPDatabase_Finished(
      parse_success, reader.albums(), reader.folders()));
  ReleaseProcessIfNeeded();
}

void ExtensionsHandler::OnIndexPicasaAlbumsContents(
    const picasa::AlbumUIDSet& album_uids,
    const std::vector<picasa::FolderINIContents>& folders_inis) {
  picasa::PicasaAlbumsIndexer indexer(album_uids);
  indexer.ParseFolderINI(folders_inis);

  Send(new ChromeUtilityHostMsg_IndexPicasaAlbumsContents_Finished(
      indexer.albums_images()));
  ReleaseProcessIfNeeded();
}
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

#if defined(OS_WIN)
void ExtensionsHandler::OnGetWiFiCredentials(const std::string& network_guid) {
  std::unique_ptr<wifi::WiFiService> wifi_service(wifi::WiFiService::Create());
  wifi_service->Initialize(NULL);

  std::string key_data;
  std::string error;
  wifi_service->GetKeyFromSystem(network_guid, &key_data, &error);

  Send(new ChromeUtilityHostMsg_GotWiFiCredentials(key_data, error.empty()));
}
#endif  // defined(OS_WIN)

}  // namespace extensions
