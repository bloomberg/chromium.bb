// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/chrome_content_utility_client.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/time/time.h"
#include "chrome/common/chrome_utility_messages.h"
#include "chrome/common/extensions/chrome_extensions_client.h"
#include "chrome/common/extensions/update_manifest.h"
#include "chrome/common/safe_browsing/zip_analyzer.h"
#include "chrome/utility/chrome_content_utility_ipc_whitelist.h"
#include "chrome/utility/extensions/unpacker.h"
#include "chrome/utility/image_writer/image_writer_handler.h"
#include "chrome/utility/profile_import_handler.h"
#include "chrome/utility/web_resource_unpacker.h"
#include "content/public/child/image_decoder_utils.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/utility/utility_thread.h"
#include "courgette/courgette.h"
#include "courgette/third_party/bsdiff.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/manifest.h"
#include "media/base/media.h"
#include "media/base/media_file_checker.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/zlib/google/zip.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

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

#if !defined(OS_ANDROID) && !defined(OS_IOS)
#include "chrome/common/media_galleries/metadata_types.h"
#include "chrome/utility/media_galleries/image_metadata_extractor.h"
#include "chrome/utility/media_galleries/ipc_data_source.h"
#include "chrome/utility/media_galleries/media_metadata_parser.h"
#endif  // !defined(OS_ANDROID) && !defined(OS_IOS)

#if defined(ENABLE_FULL_PRINTING)
#include "chrome/utility/printing_handler.h"
#endif

#if defined(ENABLE_MDNS)
#include "chrome/utility/local_discovery/service_discovery_message_handler.h"
#endif

namespace {

bool Send(IPC::Message* message) {
  return content::UtilityThread::Get()->Send(message);
}

void ReleaseProcessIfNeeded() {
  content::UtilityThread::Get()->ReleaseProcessIfNeeded();
}

#if !defined(OS_ANDROID) && !defined(OS_IOS)
void FinishParseMediaMetadata(
    metadata::MediaMetadataParser* parser,
    const extensions::api::media_galleries::MediaMetadata& metadata,
    const std::vector<metadata::AttachedImage>& attached_images) {
  Send(new ChromeUtilityHostMsg_ParseMediaMetadata_Finished(
      true, *metadata.ToValue(), attached_images));
  ReleaseProcessIfNeeded();
}
#endif  // !defined(OS_ANDROID) && !defined(OS_IOS)

}  // namespace

ChromeContentUtilityClient::ChromeContentUtilityClient()
    : filter_messages_(false) {
#if !defined(OS_ANDROID)
  handlers_.push_back(new ProfileImportHandler());
#endif

#if defined(ENABLE_FULL_PRINTING)
  handlers_.push_back(new PrintingHandler());
#endif

#if defined(ENABLE_MDNS)
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUtilityProcessEnableMDns)) {
    handlers_.push_back(new local_discovery::ServiceDiscoveryMessageHandler());
  }
#endif

  handlers_.push_back(new image_writer::ImageWriterHandler());
}

ChromeContentUtilityClient::~ChromeContentUtilityClient() {
}

void ChromeContentUtilityClient::UtilityThreadStarted() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  std::string lang = command_line->GetSwitchValueASCII(switches::kLang);
  if (!lang.empty())
    extension_l10n_util::SetProcessLocale(lang);

  if (command_line->HasSwitch(switches::kUtilityProcessRunningElevated)) {
    message_id_whitelist_.insert(kMessageWhitelist,
                                 kMessageWhitelist + kMessageWhitelistSize);
    filter_messages_ = true;
  }
}

bool ChromeContentUtilityClient::OnMessageReceived(
    const IPC::Message& message) {
  if (filter_messages_ && !ContainsKey(message_id_whitelist_, message.type()))
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChromeContentUtilityClient, message)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_UnpackExtension, OnUnpackExtension)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_UnpackWebResource,
                        OnUnpackWebResource)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_ParseUpdateManifest,
                        OnParseUpdateManifest)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_DecodeImage, OnDecodeImage)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_DecodeImageBase64, OnDecodeImageBase64)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_RobustJPEGDecodeImage,
                        OnRobustJPEGDecodeImage)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_ParseJSON, OnParseJSON)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_PatchFileBsdiff,
                        OnPatchFileBsdiff)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_PatchFileCourgette,
                        OnPatchFileCourgette)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_StartupPing, OnStartupPing)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_AnalyzeZipFileForDownloadProtection,
                        OnAnalyzeZipFileForDownloadProtection)

#if !defined(OS_ANDROID) && !defined(OS_IOS)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_CheckMediaFile, OnCheckMediaFile)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_ParseMediaMetadata,
                        OnParseMediaMetadata)
#endif  // !defined(OS_ANDROID) && !defined(OS_IOS)

#if defined(OS_CHROMEOS)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_CreateZipFile, OnCreateZipFile)
#endif  // defined(OS_CHROMEOS)

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

  for (Handlers::iterator it = handlers_.begin();
       !handled && it != handlers_.end(); ++it) {
    handled = (*it)->OnMessageReceived(message);
  }

  return handled;
}

// static
void ChromeContentUtilityClient::PreSandboxStartup() {
#if defined(ENABLE_FULL_PRINTING)
  PrintingHandler::PreSandboxStartup();
#endif

#if defined(ENABLE_MDNS)
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUtilityProcessEnableMDns)) {
    local_discovery::ServiceDiscoveryMessageHandler::PreSandboxStartup();
  }
#endif  // ENABLE_MDNS

#if !defined(OS_ANDROID) && !defined(OS_IOS)
  // Initialize libexif for image metadata parsing.
  metadata::ImageMetadataExtractor::InitializeLibrary();
#endif  // !defined(OS_ANDROID) && !defined(OS_IOS)

  // Load media libraries for media file validation.
  base::FilePath media_path;
  PathService::Get(content::DIR_MEDIA_LIBS, &media_path);
  if (!media_path.empty())
    media::InitializeMediaLibrary(media_path);
}

void ChromeContentUtilityClient::OnUnpackExtension(
    const base::FilePath& extension_path,
    const std::string& extension_id,
    int location,
    int creation_flags) {
  CHECK_GT(location, extensions::Manifest::INVALID_LOCATION);
  CHECK_LT(location, extensions::Manifest::NUM_LOCATIONS);
  extensions::ExtensionsClient::Set(
      extensions::ChromeExtensionsClient::GetInstance());
  extensions::Unpacker unpacker(
      extension_path,
      extension_id,
      static_cast<extensions::Manifest::Location>(location),
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

void ChromeContentUtilityClient::OnUnpackWebResource(
    const std::string& resource_data) {
  // Parse json data.
  // TODO(mrc): Add the possibility of a template that controls parsing, and
  // the ability to download and verify images.
  WebResourceUnpacker unpacker(resource_data);
  if (unpacker.Run()) {
    Send(new ChromeUtilityHostMsg_UnpackWebResource_Succeeded(
        *unpacker.parsed_json()));
  } else {
    Send(new ChromeUtilityHostMsg_UnpackWebResource_Failed(
        unpacker.error_message()));
  }

  ReleaseProcessIfNeeded();
}

void ChromeContentUtilityClient::OnParseUpdateManifest(const std::string& xml) {
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

void ChromeContentUtilityClient::OnDecodeImage(
    const std::vector<unsigned char>& encoded_data) {
  const SkBitmap& decoded_image = content::DecodeImage(&encoded_data[0],
                                                       gfx::Size(),
                                                       encoded_data.size());
  if (decoded_image.empty()) {
    Send(new ChromeUtilityHostMsg_DecodeImage_Failed());
  } else {
    Send(new ChromeUtilityHostMsg_DecodeImage_Succeeded(decoded_image));
  }
  ReleaseProcessIfNeeded();
}

void ChromeContentUtilityClient::OnDecodeImageBase64(
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

  OnDecodeImage(decoded_vector);
}

#if defined(OS_CHROMEOS)
void ChromeContentUtilityClient::OnCreateZipFile(
    const base::FilePath& src_dir,
    const std::vector<base::FilePath>& src_relative_paths,
    const base::FileDescriptor& dest_fd) {
  bool succeeded = true;

  // Check sanity of source relative paths. Reject if path is absolute or
  // contains any attempt to reference a parent directory ("../" tricks).
  for (std::vector<base::FilePath>::const_iterator iter =
           src_relative_paths.begin(); iter != src_relative_paths.end();
       ++iter) {
    if (iter->IsAbsolute() || iter->ReferencesParent()) {
      succeeded = false;
      break;
    }
  }

  if (succeeded)
    succeeded = zip::ZipFiles(src_dir, src_relative_paths, dest_fd.fd);

  if (succeeded)
    Send(new ChromeUtilityHostMsg_CreateZipFile_Succeeded());
  else
    Send(new ChromeUtilityHostMsg_CreateZipFile_Failed());
  ReleaseProcessIfNeeded();
}
#endif  // defined(OS_CHROMEOS)

void ChromeContentUtilityClient::OnRobustJPEGDecodeImage(
    const std::vector<unsigned char>& encoded_data) {
  // Our robust jpeg decoding is using IJG libjpeg.
  if (gfx::JPEGCodec::JpegLibraryVariant() == gfx::JPEGCodec::IJG_LIBJPEG) {
    scoped_ptr<SkBitmap> decoded_image(gfx::JPEGCodec::Decode(
        &encoded_data[0], encoded_data.size()));
    if (!decoded_image.get() || decoded_image->empty()) {
      Send(new ChromeUtilityHostMsg_DecodeImage_Failed());
    } else {
      Send(new ChromeUtilityHostMsg_DecodeImage_Succeeded(*decoded_image));
    }
  } else {
    Send(new ChromeUtilityHostMsg_DecodeImage_Failed());
  }
  ReleaseProcessIfNeeded();
}

void ChromeContentUtilityClient::OnParseJSON(const std::string& json) {
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

void ChromeContentUtilityClient::OnPatchFileBsdiff(
    const base::FilePath& input_file,
    const base::FilePath& patch_file,
    const base::FilePath& output_file) {
  if (input_file.empty() || patch_file.empty() || output_file.empty()) {
    Send(new ChromeUtilityHostMsg_PatchFile_Failed(-1));
  } else {
    const int patch_status = courgette::ApplyBinaryPatch(input_file,
                                                         patch_file,
                                                         output_file);
    if (patch_status != courgette::OK)
      Send(new ChromeUtilityHostMsg_PatchFile_Failed(patch_status));
    else
      Send(new ChromeUtilityHostMsg_PatchFile_Succeeded());
  }
  ReleaseProcessIfNeeded();
}

void ChromeContentUtilityClient::OnPatchFileCourgette(
    const base::FilePath& input_file,
    const base::FilePath& patch_file,
    const base::FilePath& output_file) {
  if (input_file.empty() || patch_file.empty() || output_file.empty()) {
    Send(new ChromeUtilityHostMsg_PatchFile_Failed(-1));
  } else {
    const int patch_status = courgette::ApplyEnsemblePatch(
        input_file.value().c_str(),
        patch_file.value().c_str(),
        output_file.value().c_str());
    if (patch_status != courgette::C_OK)
      Send(new ChromeUtilityHostMsg_PatchFile_Failed(patch_status));
    else
      Send(new ChromeUtilityHostMsg_PatchFile_Succeeded());
  }
  ReleaseProcessIfNeeded();
}

void ChromeContentUtilityClient::OnStartupPing() {
  Send(new ChromeUtilityHostMsg_ProcessStarted);
  // Don't release the process, we assume further messages are on the way.
}

void ChromeContentUtilityClient::OnAnalyzeZipFileForDownloadProtection(
    const IPC::PlatformFileForTransit& zip_file) {
  safe_browsing::zip_analyzer::Results results;
  safe_browsing::zip_analyzer::AnalyzeZipFile(
      IPC::PlatformFileForTransitToFile(zip_file), &results);
  Send(new ChromeUtilityHostMsg_AnalyzeZipFileForDownloadProtection_Finished(
      results));
  ReleaseProcessIfNeeded();
}

#if !defined(OS_ANDROID) && !defined(OS_IOS)
void ChromeContentUtilityClient::OnCheckMediaFile(
    int64 milliseconds_of_decoding,
    const IPC::PlatformFileForTransit& media_file) {
  media::MediaFileChecker checker(
      IPC::PlatformFileForTransitToFile(media_file));
  const bool check_success = checker.Start(
      base::TimeDelta::FromMilliseconds(milliseconds_of_decoding));
  Send(new ChromeUtilityHostMsg_CheckMediaFile_Finished(check_success));
  ReleaseProcessIfNeeded();
}

void ChromeContentUtilityClient::OnParseMediaMetadata(
    const std::string& mime_type, int64 total_size, bool get_attached_images) {
  // Only one IPCDataSource may be created and added to the list of handlers.
  metadata::IPCDataSource* source = new metadata::IPCDataSource(total_size);
  handlers_.push_back(source);

  metadata::MediaMetadataParser* parser = new metadata::MediaMetadataParser(
      source, mime_type, get_attached_images);
  parser->Start(base::Bind(&FinishParseMediaMetadata, base::Owned(parser)));
}
#endif  // !defined(OS_ANDROID) && !defined(OS_IOS)

#if defined(OS_WIN)
void ChromeContentUtilityClient::OnParseITunesPrefXml(
    const std::string& itunes_xml_data) {
  base::FilePath library_path(
      itunes::FindLibraryLocationInPrefXml(itunes_xml_data));
  Send(new ChromeUtilityHostMsg_GotITunesDirectory(library_path));
  ReleaseProcessIfNeeded();
}
#endif  // defined(OS_WIN)

#if defined(OS_MACOSX)
void ChromeContentUtilityClient::OnParseIPhotoLibraryXmlFile(
    const IPC::PlatformFileForTransit& iphoto_library_file) {
  iphoto::IPhotoLibraryParser parser;
  base::File file = IPC::PlatformFileForTransitToFile(iphoto_library_file);
  bool result = parser.Parse(iapps::ReadFileAsString(file.Pass()));
  Send(new ChromeUtilityHostMsg_GotIPhotoLibrary(result, parser.library()));
  ReleaseProcessIfNeeded();
}
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN) || defined(OS_MACOSX)
void ChromeContentUtilityClient::OnParseITunesLibraryXmlFile(
    const IPC::PlatformFileForTransit& itunes_library_file) {
  itunes::ITunesLibraryParser parser;
  base::File file = IPC::PlatformFileForTransitToFile(itunes_library_file);
  bool result = parser.Parse(iapps::ReadFileAsString(file.Pass()));
  Send(new ChromeUtilityHostMsg_GotITunesLibrary(result, parser.library()));
  ReleaseProcessIfNeeded();
}

void ChromeContentUtilityClient::OnParsePicasaPMPDatabase(
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

void ChromeContentUtilityClient::OnIndexPicasaAlbumsContents(
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
void ChromeContentUtilityClient::OnGetAndEncryptWiFiCredentials(
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
    NetworkingPrivateCrypto crypto;
    success = crypto.EncryptByteString(public_key, key_data, &ciphertext);
  }

  Send(new ChromeUtilityHostMsg_GotEncryptedWiFiCredentials(ciphertext,
                                                            success));
}
#endif  // defined(OS_WIN)
