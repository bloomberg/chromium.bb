// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/chrome_content_utility_client.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "chrome/common/chrome_utility_messages.h"
#include "chrome/common/safe_browsing/zip_analyzer.h"
#include "chrome/utility/chrome_content_utility_ipc_whitelist.h"
#include "chrome/utility/utility_message_handler.h"
#include "chrome/utility/web_resource_unpacker.h"
#include "content/public/child/image_decoder_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/utility/utility_thread.h"
#include "courgette/courgette.h"
#include "courgette/third_party/bsdiff.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/zlib/google/zip.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/size.h"

#if !defined(OS_ANDROID)
#include "chrome/utility/profile_import_handler.h"
#endif

#if defined(ENABLE_EXTENSIONS)
#include "chrome/common/extensions/chrome_utility_extensions_messages.h"
#include "chrome/utility/extensions/extensions_handler.h"
#include "chrome/utility/image_writer/image_writer_handler.h"
#include "chrome/utility/media_galleries/ipc_data_source.h"
#include "chrome/utility/media_galleries/media_metadata_parser.h"
#endif

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

#if defined(ENABLE_EXTENSIONS)
void FinishParseMediaMetadata(
    metadata::MediaMetadataParser* /* parser */,
    const extensions::api::media_galleries::MediaMetadata& metadata,
    const std::vector<metadata::AttachedImage>& attached_images) {
  Send(new ChromeUtilityHostMsg_ParseMediaMetadata_Finished(
      true, *metadata.ToValue(), attached_images));
  ReleaseProcessIfNeeded();
}
#endif

}  // namespace

ChromeContentUtilityClient::ChromeContentUtilityClient()
    : filter_messages_(false) {
#if !defined(OS_ANDROID)
  handlers_.push_back(new ProfileImportHandler());
#endif

#if defined(ENABLE_EXTENSIONS)
  handlers_.push_back(new extensions::ExtensionsHandler());
  handlers_.push_back(new image_writer::ImageWriterHandler());
#endif

#if defined(ENABLE_FULL_PRINTING)
  handlers_.push_back(new PrintingHandler());
#endif

#if defined(ENABLE_MDNS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUtilityProcessEnableMDns)) {
    handlers_.push_back(new local_discovery::ServiceDiscoveryMessageHandler());
  }
#endif
}

ChromeContentUtilityClient::~ChromeContentUtilityClient() {
}

void ChromeContentUtilityClient::UtilityThreadStarted() {
#if defined(ENABLE_EXTENSIONS)
  extensions::ExtensionsHandler::UtilityThreadStarted();
#endif

  if (kMessageWhitelistSize > 0) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kUtilityProcessRunningElevated)) {
      message_id_whitelist_.insert(kMessageWhitelist,
                                   kMessageWhitelist + kMessageWhitelistSize);
      filter_messages_ = true;
    }
  }
}

bool ChromeContentUtilityClient::OnMessageReceived(
    const IPC::Message& message) {
  if (filter_messages_ && !ContainsKey(message_id_whitelist_, message.type()))
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChromeContentUtilityClient, message)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_UnpackWebResource,
                        OnUnpackWebResource)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_DecodeImage, OnDecodeImage)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_RobustJPEGDecodeImage,
                        OnRobustJPEGDecodeImage)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_PatchFileBsdiff,
                        OnPatchFileBsdiff)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_PatchFileCourgette,
                        OnPatchFileCourgette)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_StartupPing, OnStartupPing)
#if defined(FULL_SAFE_BROWSING)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_AnalyzeZipFileForDownloadProtection,
                        OnAnalyzeZipFileForDownloadProtection)
#endif
#if defined(ENABLE_EXTENSIONS)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_ParseMediaMetadata,
                        OnParseMediaMetadata)
#endif
#if defined(OS_CHROMEOS)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_CreateZipFile, OnCreateZipFile)
#endif
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
#if defined(ENABLE_EXTENSIONS)
  extensions::ExtensionsHandler::PreSandboxStartup();
#endif

#if defined(ENABLE_FULL_PRINTING)
  PrintingHandler::PreSandboxStartup();
#endif

#if defined(ENABLE_MDNS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUtilityProcessEnableMDns)) {
    local_discovery::ServiceDiscoveryMessageHandler::PreSandboxStartup();
  }
#endif  // ENABLE_MDNS
}

// static
void ChromeContentUtilityClient::DecodeImage(
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

void ChromeContentUtilityClient::OnDecodeImage(
    const std::vector<unsigned char>& encoded_data) {
  DecodeImage(encoded_data);
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

#if defined(FULL_SAFE_BROWSING)
void ChromeContentUtilityClient::OnAnalyzeZipFileForDownloadProtection(
    const IPC::PlatformFileForTransit& zip_file) {
  safe_browsing::zip_analyzer::Results results;
  safe_browsing::zip_analyzer::AnalyzeZipFile(
      IPC::PlatformFileForTransitToFile(zip_file), &results);
  Send(new ChromeUtilityHostMsg_AnalyzeZipFileForDownloadProtection_Finished(
      results));
  ReleaseProcessIfNeeded();
}
#endif

#if defined(ENABLE_EXTENSIONS)
// TODO(thestig): Try to move this to
// chrome/utility/extensions/extensions_handler.cc.
void ChromeContentUtilityClient::OnParseMediaMetadata(
    const std::string& mime_type, int64 total_size, bool get_attached_images) {
  // Only one IPCDataSource may be created and added to the list of handlers.
  metadata::IPCDataSource* source = new metadata::IPCDataSource(total_size);
  handlers_.push_back(source);

  metadata::MediaMetadataParser* parser = new metadata::MediaMetadataParser(
      source, mime_type, get_attached_images);
  parser->Start(base::Bind(&FinishParseMediaMetadata, base::Owned(parser)));
}
#endif
