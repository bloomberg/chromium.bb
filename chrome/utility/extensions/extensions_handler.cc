// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/extensions/extensions_handler.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "chrome/common/chrome_utility_messages.h"
#include "chrome/common/extensions/chrome_extensions_client.h"
#include "chrome/common/extensions/chrome_utility_extensions_messages.h"
#include "chrome/common/media_galleries/metadata_types.h"
#include "chrome/utility/chrome_content_utility_client.h"
#include "chrome/utility/extensions/unpacker.h"
#include "chrome/utility/media_galleries/image_metadata_extractor.h"
#include "content/public/common/content_paths.h"
#include "content/public/utility/utility_thread.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/manifest.h"
#include "extensions/common/update_manifest.h"
#include "media/base/media.h"
#include "media/base/media_file_checker.h"
#include "ui/base/ui_base_switches.h"

#if defined(OS_WIN)
#include "chrome/common/extensions/api/networking_private/networking_private_crypto.h"
#include "chrome/utility/media_galleries/itunes_pref_parser_win.h"
#include "components/wifi/wifi_service.h"
#endif  // defined(OS_WIN)

#if defined(OS_MACOSX)
#include "chrome/utility/media_galleries/iphoto_library_parser.h"
#endif  // defined(OS_MACOSX)

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

}  // namespace

ExtensionsHandler::ExtensionsHandler() {}

ExtensionsHandler::~ExtensionsHandler() {}

// static
void ExtensionsHandler::PreSandboxStartup() {
  // Initialize libexif for image metadata parsing.
  metadata::ImageMetadataExtractor::InitializeLibrary();

  // Load media libraries for media file validation.
  base::FilePath media_path;
  PathService::Get(content::DIR_MEDIA_LIBS, &media_path);
  if (!media_path.empty())
    media::InitializeMediaLibrary(media_path);
}

void ExtensionsHandler::UtilityThreadStarted() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::string lang = command_line->GetSwitchValueASCII(switches::kLang);
  if (!lang.empty())
    extension_l10n_util::SetProcessLocale(lang);
}

bool ExtensionsHandler::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExtensionsHandler, message)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_UnpackExtension, OnUnpackExtension)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_ParseUpdateManifest,
                        OnParseUpdateManifest)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_DecodeImageBase64, OnDecodeImageBase64)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_ParseJSON, OnParseJSON)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_CheckMediaFile, OnCheckMediaFile)
#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_ParseITunesPrefXml,
                        OnParseITunesPrefXml)
#endif  // defined(OS_WIN)

#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_ParseIPhotoLibraryXmlFile,
                        OnParseIPhotoLibraryXmlFile)
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN) || defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_ParseITunesLibraryXmlFile,
                        OnParseITunesLibraryXmlFile)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_ParsePicasaPMPDatabase,
                        OnParsePicasaPMPDatabase)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_IndexPicasaAlbumsContents,
                        OnIndexPicasaAlbumsContents)
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER(ChromeUtilityHostMsg_GetAndEncryptWiFiCredentials,
                        OnGetAndEncryptWiFiCredentials)
#endif  // defined(OS_WIN)

    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ExtensionsHandler::OnUnpackExtension(
    const base::FilePath& extension_path,
    const std::string& extension_id,
    int location,
    int creation_flags) {
  CHECK_GT(location, Manifest::INVALID_LOCATION);
  CHECK_LT(location, Manifest::NUM_LOCATIONS);
  ExtensionsClient::Set(ChromeExtensionsClient::GetInstance());
  Unpacker unpacker(extension_path,
                    extension_id,
                    static_cast<Manifest::Location>(location),
                    creation_flags);
  if (unpacker.Run() && unpacker.DumpImagesToFile() &&
      unpacker.DumpMessageCatalogsToFile()) {
    Send(new ChromeUtilityHostMsg_UnpackExtension_Succeeded(
        *unpacker.parsed_manifest()));
  } else {
    Send(new ChromeUtilityHostMsg_UnpackExtension_Failed(
        unpacker.error_message()));
  }

  ReleaseProcessIfNeeded();
}

void ExtensionsHandler::OnParseUpdateManifest(const std::string& xml) {
  UpdateManifest manifest;
  if (!manifest.Parse(xml)) {
    Send(new ChromeUtilityHostMsg_ParseUpdateManifest_Failed(
        manifest.errors()));
  } else {
    Send(new ChromeUtilityHostMsg_ParseUpdateManifest_Succeeded(
        manifest.results()));
  }
  ReleaseProcessIfNeeded();
}

void ExtensionsHandler::OnDecodeImageBase64(
    const std::string& encoded_string) {
  std::string decoded_string;

  if (!base::Base64Decode(encoded_string, &decoded_string)) {
    Send(new ChromeUtilityHostMsg_DecodeImage_Failed());
    return;
  }

  std::vector<unsigned char> decoded_vector(decoded_string.size());
  for (size_t i = 0; i < decoded_string.size(); ++i) {
    decoded_vector[i] = static_cast<unsigned char>(decoded_string[i]);
  }

  ChromeContentUtilityClient::DecodeImage(decoded_vector);
}

void ExtensionsHandler::OnParseJSON(const std::string& json) {
  int error_code;
  std::string error;
  base::Value* value = base::JSONReader::ReadAndReturnError(
      json, base::JSON_PARSE_RFC, &error_code, &error);
  if (value) {
    base::ListValue wrapper;
    wrapper.Append(value);
    Send(new ChromeUtilityHostMsg_ParseJSON_Succeeded(wrapper));
  } else {
    Send(new ChromeUtilityHostMsg_ParseJSON_Failed(error));
  }
  ReleaseProcessIfNeeded();
}

void ExtensionsHandler::OnCheckMediaFile(
    int64 milliseconds_of_decoding,
    const IPC::PlatformFileForTransit& media_file) {
  media::MediaFileChecker checker(
      IPC::PlatformFileForTransitToFile(media_file));
  const bool check_success = checker.Start(
      base::TimeDelta::FromMilliseconds(milliseconds_of_decoding));
  Send(new ChromeUtilityHostMsg_CheckMediaFile_Finished(check_success));
  ReleaseProcessIfNeeded();
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

#if defined(OS_MACOSX)
void ExtensionsHandler::OnParseIPhotoLibraryXmlFile(
    const IPC::PlatformFileForTransit& iphoto_library_file) {
  iphoto::IPhotoLibraryParser parser;
  base::File file = IPC::PlatformFileForTransitToFile(iphoto_library_file);
  bool result = parser.Parse(iapps::ReadFileAsString(file.Pass()));
  Send(new ChromeUtilityHostMsg_GotIPhotoLibrary(result, parser.library()));
  ReleaseProcessIfNeeded();
}
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN) || defined(OS_MACOSX)
void ExtensionsHandler::OnParseITunesLibraryXmlFile(
    const IPC::PlatformFileForTransit& itunes_library_file) {
  itunes::ITunesLibraryParser parser;
  base::File file = IPC::PlatformFileForTransitToFile(itunes_library_file);
  bool result = parser.Parse(iapps::ReadFileAsString(file.Pass()));
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

  picasa::PicasaAlbumTableReader reader(files.Pass());
  bool parse_success = reader.Init();
  Send(new ChromeUtilityHostMsg_ParsePicasaPMPDatabase_Finished(
      parse_success,
      reader.albums(),
      reader.folders()));
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
void ExtensionsHandler::OnGetAndEncryptWiFiCredentials(
    const std::string& network_guid,
    const std::vector<uint8>& public_key) {
  scoped_ptr<wifi::WiFiService> wifi_service(wifi::WiFiService::Create());
  wifi_service->Initialize(NULL);

  std::string key_data;
  std::string error;
  wifi_service->GetKeyFromSystem(network_guid, &key_data, &error);

  std::vector<uint8> ciphertext;
  bool success = error.empty() && !key_data.empty();
  if (success) {
    success = networking_private_crypto::EncryptByteString(
        public_key, key_data, &ciphertext);
  }

  Send(new ChromeUtilityHostMsg_GotEncryptedWiFiCredentials(ciphertext,
                                                            success));
}
#endif  // defined(OS_WIN)

}  // namespace extensions
