// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/chrome_content_utility_client.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "base/time/time.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_utility_messages.h"
#include "chrome/common/extensions/chrome_extensions_client.h"
#include "chrome/common/extensions/extension_l10n_util.h"
#include "chrome/common/extensions/update_manifest.h"
#include "chrome/common/safe_browsing/zip_analyzer.h"
#include "chrome/utility/chrome_content_utility_ipc_whitelist.h"
#include "chrome/utility/cloud_print/bitmap_image.h"
#include "chrome/utility/cloud_print/pwg_encoder.h"
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
#include "extensions/common/manifest.h"
#include "media/base/media.h"
#include "media/base/media_file_checker.h"
#include "printing/page_range.h"
#include "printing/pdf_render_settings.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/zlib/google/zip.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

#if defined(OS_WIN)
#include "base/win/iat_patch_function.h"
#include "base/win/scoped_handle.h"
#include "chrome/common/extensions/api/networking_private/networking_private_crypto.h"
#include "chrome/utility/media_galleries/itunes_pref_parser_win.h"
#include "components/wifi/wifi_service.h"
#include "printing/emf_win.h"
#include "ui/gfx/gdi_util.h"
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
#include "chrome/utility/media_galleries/ipc_data_source.h"
#include "chrome/utility/media_galleries/media_metadata_parser.h"
#endif  // !defined(OS_ANDROID) && !defined(OS_IOS)

#if defined(ENABLE_FULL_PRINTING)
#include "chrome/common/crash_keys.h"
#include "printing/backend/print_backend.h"
#endif

#if defined(ENABLE_MDNS)
#include "chrome/utility/local_discovery/service_discovery_message_handler.h"
#endif  // ENABLE_MDNS

namespace chrome {

namespace {

bool Send(IPC::Message* message) {
  return content::UtilityThread::Get()->Send(message);
}

void ReleaseProcessIfNeeded() {
  content::UtilityThread::Get()->ReleaseProcessIfNeeded();
}

class PdfFunctionsBase {
 public:
  PdfFunctionsBase() : render_pdf_to_bitmap_func_(NULL),
                       get_pdf_doc_info_func_(NULL) {}

  bool Init() {
    base::FilePath pdf_module_path;
    if (!PathService::Get(chrome::FILE_PDF_PLUGIN, &pdf_module_path) ||
        !base::PathExists(pdf_module_path)) {
      return false;
    }

    pdf_lib_.Reset(base::LoadNativeLibrary(pdf_module_path, NULL));
    if (!pdf_lib_.is_valid()) {
      LOG(WARNING) << "Couldn't load PDF plugin";
      return false;
    }

    render_pdf_to_bitmap_func_ =
        reinterpret_cast<RenderPDFPageToBitmapProc>(
            pdf_lib_.GetFunctionPointer("RenderPDFPageToBitmap"));
    LOG_IF(WARNING, !render_pdf_to_bitmap_func_) <<
        "Missing RenderPDFPageToBitmap";

    get_pdf_doc_info_func_ =
        reinterpret_cast<GetPDFDocInfoProc>(
            pdf_lib_.GetFunctionPointer("GetPDFDocInfo"));
    LOG_IF(WARNING, !get_pdf_doc_info_func_) << "Missing GetPDFDocInfo";

    if (!render_pdf_to_bitmap_func_ || !get_pdf_doc_info_func_ ||
        !PlatformInit(pdf_module_path, pdf_lib_)) {
      Reset();
    }

    return IsValid();
  }

  bool IsValid() const {
    return pdf_lib_.is_valid();
  }

  void Reset() {
    pdf_lib_.Reset(NULL);
  }

  bool RenderPDFPageToBitmap(const void* pdf_buffer,
                             int pdf_buffer_size,
                             int page_number,
                             void* bitmap_buffer,
                             int bitmap_width,
                             int bitmap_height,
                             int dpi_x,
                             int dpi_y,
                             bool autorotate) {
    if (!render_pdf_to_bitmap_func_)
      return false;
    return render_pdf_to_bitmap_func_(pdf_buffer, pdf_buffer_size, page_number,
                                      bitmap_buffer, bitmap_width,
                                      bitmap_height, dpi_x, dpi_y, autorotate);
  }

  bool GetPDFDocInfo(const void* pdf_buffer,
                     int buffer_size,
                     int* page_count,
                     double* max_page_width) {
    if (!get_pdf_doc_info_func_)
      return false;
    return get_pdf_doc_info_func_(pdf_buffer, buffer_size, page_count,
                                  max_page_width);
  }

 protected:
  virtual bool PlatformInit(
      const base::FilePath& pdf_module_path,
      const base::ScopedNativeLibrary& pdf_lib) {
    return true;
  };

 private:
  // Exported by PDF plugin.
  typedef bool (*RenderPDFPageToBitmapProc)(const void* pdf_buffer,
                                            int pdf_buffer_size,
                                            int page_number,
                                            void* bitmap_buffer,
                                            int bitmap_width,
                                            int bitmap_height,
                                            int dpi_x,
                                            int dpi_y,
                                            bool autorotate);
  typedef bool (*GetPDFDocInfoProc)(const void* pdf_buffer,
                                    int buffer_size, int* page_count,
                                    double* max_page_width);

  RenderPDFPageToBitmapProc render_pdf_to_bitmap_func_;
  GetPDFDocInfoProc get_pdf_doc_info_func_;

  base::ScopedNativeLibrary pdf_lib_;
  DISALLOW_COPY_AND_ASSIGN(PdfFunctionsBase);
};

#if defined(OS_WIN)
// The 2 below IAT patch functions are almost identical to the code in
// render_process_impl.cc. This is needed to work around specific Windows APIs
// used by the Chrome PDF plugin that will fail in the sandbox.
static base::win::IATPatchFunction g_iat_patch_createdca;
HDC WINAPI UtilityProcess_CreateDCAPatch(LPCSTR driver_name,
                                         LPCSTR device_name,
                                         LPCSTR output,
                                         const DEVMODEA* init_data) {
  if (driver_name && (std::string("DISPLAY") == driver_name)) {
    // CreateDC fails behind the sandbox, but not CreateCompatibleDC.
    return CreateCompatibleDC(NULL);
  }

  NOTREACHED();
  return CreateDCA(driver_name, device_name, output, init_data);
}

static base::win::IATPatchFunction g_iat_patch_get_font_data;
DWORD WINAPI UtilityProcess_GetFontDataPatch(
    HDC hdc, DWORD table, DWORD offset, LPVOID buffer, DWORD length) {
  int rv = GetFontData(hdc, table, offset, buffer, length);
  if (rv == GDI_ERROR && hdc) {
    HFONT font = static_cast<HFONT>(GetCurrentObject(hdc, OBJ_FONT));

    LOGFONT logfont;
    if (GetObject(font, sizeof(LOGFONT), &logfont)) {
      content::UtilityThread::Get()->PreCacheFont(logfont);
      rv = GetFontData(hdc, table, offset, buffer, length);
      content::UtilityThread::Get()->ReleaseCachedFonts();
    }
  }
  return rv;
}

class PdfFunctionsWin : public PdfFunctionsBase {
 public:
  PdfFunctionsWin() : render_pdf_to_dc_func_(NULL) {
  }

  bool PlatformInit(
      const base::FilePath& pdf_module_path,
      const base::ScopedNativeLibrary& pdf_lib) OVERRIDE {
    // Patch the IAT for handling specific APIs known to fail in the sandbox.
    if (!g_iat_patch_createdca.is_patched()) {
      g_iat_patch_createdca.Patch(pdf_module_path.value().c_str(),
                                  "gdi32.dll", "CreateDCA",
                                  UtilityProcess_CreateDCAPatch);
    }

    if (!g_iat_patch_get_font_data.is_patched()) {
      g_iat_patch_get_font_data.Patch(pdf_module_path.value().c_str(),
                                      "gdi32.dll", "GetFontData",
                                      UtilityProcess_GetFontDataPatch);
    }
    render_pdf_to_dc_func_ =
      reinterpret_cast<RenderPDFPageToDCProc>(
          pdf_lib.GetFunctionPointer("RenderPDFPageToDC"));
    LOG_IF(WARNING, !render_pdf_to_dc_func_) << "Missing RenderPDFPageToDC";

    return render_pdf_to_dc_func_ != NULL;
  }

  bool RenderPDFPageToDC(const void* pdf_buffer,
                         int buffer_size,
                         int page_number,
                         HDC dc,
                         int dpi_x,
                         int dpi_y,
                         int bounds_origin_x,
                         int bounds_origin_y,
                         int bounds_width,
                         int bounds_height,
                         bool fit_to_bounds,
                         bool stretch_to_bounds,
                         bool keep_aspect_ratio,
                         bool center_in_bounds,
                         bool autorotate) {
    if (!render_pdf_to_dc_func_)
      return false;
    return render_pdf_to_dc_func_(pdf_buffer, buffer_size, page_number,
                                  dc, dpi_x, dpi_y, bounds_origin_x,
                                  bounds_origin_y, bounds_width, bounds_height,
                                  fit_to_bounds, stretch_to_bounds,
                                  keep_aspect_ratio, center_in_bounds,
                                  autorotate);
  }

 private:
  // Exported by PDF plugin.
  typedef bool (*RenderPDFPageToDCProc)(
      const void* pdf_buffer, int buffer_size, int page_number, HDC dc,
      int dpi_x, int dpi_y, int bounds_origin_x, int bounds_origin_y,
      int bounds_width, int bounds_height, bool fit_to_bounds,
      bool stretch_to_bounds, bool keep_aspect_ratio, bool center_in_bounds,
      bool autorotate);
  RenderPDFPageToDCProc render_pdf_to_dc_func_;

  DISALLOW_COPY_AND_ASSIGN(PdfFunctionsWin);
};

typedef PdfFunctionsWin PdfFunctions;
#else  // OS_WIN
typedef PdfFunctionsBase PdfFunctions;
#endif  // OS_WIN

#if !defined(OS_ANDROID) && !defined(OS_IOS)
void FinishParseMediaMetadata(
    metadata::MediaMetadataParser* parser,
    scoped_ptr<extensions::api::media_galleries::MediaMetadata> metadata) {
  Send(new ChromeUtilityHostMsg_ParseMediaMetadata_Finished(
      true, *(metadata->ToValue().get())));
  ReleaseProcessIfNeeded();
}
#endif  // !defined(OS_ANDROID) && !defined(OS_IOS)

static base::LazyInstance<PdfFunctions> g_pdf_lib = LAZY_INSTANCE_INITIALIZER;

}  // namespace

ChromeContentUtilityClient::ChromeContentUtilityClient()
    : filter_messages_(false) {
#if !defined(OS_ANDROID)
  handlers_.push_back(new ProfileImportHandler());
#endif  // OS_ANDROID

#if defined(ENABLE_MDNS)
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUtilityProcessEnableMDns)) {
    handlers_.push_back(new local_discovery::ServiceDiscoveryMessageHandler());
  }
#endif  // ENABLE_MDNS

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
  if (filter_messages_ &&
      (message_id_whitelist_.find(message.type()) ==
       message_id_whitelist_.end())) {
    return false;
  }

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChromeContentUtilityClient, message)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_UnpackExtension, OnUnpackExtension)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_UnpackWebResource,
                        OnUnpackWebResource)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_ParseUpdateManifest,
                        OnParseUpdateManifest)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_DecodeImage, OnDecodeImage)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_DecodeImageBase64, OnDecodeImageBase64)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_RenderPDFPagesToMetafile,
                        OnRenderPDFPagesToMetafile)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_RenderPDFPagesToPWGRaster,
                        OnRenderPDFPagesToPWGRaster)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_RobustJPEGDecodeImage,
                        OnRobustJPEGDecodeImage)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_ParseJSON, OnParseJSON)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_GetPrinterCapsAndDefaults,
                        OnGetPrinterCapsAndDefaults)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_GetPrinterSemanticCapsAndDefaults,
                        OnGetPrinterSemanticCapsAndDefaults)
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
#if defined(ENABLE_MDNS)
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUtilityProcessEnableMDns)) {
    local_discovery::ServiceDiscoveryMessageHandler::PreSandboxStartup();
  }
#endif  // ENABLE_MDNS

  g_pdf_lib.Get().Init();

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

void ChromeContentUtilityClient::OnRenderPDFPagesToMetafile(
    base::PlatformFile pdf_file,
    const base::FilePath& metafile_path,
    const printing::PdfRenderSettings& settings,
    const std::vector<printing::PageRange>& page_ranges) {
  bool succeeded = false;
#if defined(OS_WIN)
  int highest_rendered_page_number = 0;
  double scale_factor = 1.0;
  succeeded = RenderPDFToWinMetafile(pdf_file,
                                     metafile_path,
                                     settings,
                                     page_ranges,
                                     &highest_rendered_page_number,
                                     &scale_factor);
  if (succeeded) {
    Send(new ChromeUtilityHostMsg_RenderPDFPagesToMetafile_Succeeded(
        highest_rendered_page_number, scale_factor));
  }
#endif  // defined(OS_WIN)
  if (!succeeded) {
    Send(new ChromeUtilityHostMsg_RenderPDFPagesToMetafile_Failed());
  }
  ReleaseProcessIfNeeded();
}

void ChromeContentUtilityClient::OnRenderPDFPagesToPWGRaster(
    IPC::PlatformFileForTransit pdf_transit,
    const printing::PdfRenderSettings& settings,
    const printing::PwgRasterSettings& bitmap_settings,
    IPC::PlatformFileForTransit bitmap_transit) {
  base::PlatformFile pdf =
      IPC::PlatformFileForTransitToPlatformFile(pdf_transit);
  base::PlatformFile bitmap =
      IPC::PlatformFileForTransitToPlatformFile(bitmap_transit);
  if (RenderPDFPagesToPWGRaster(pdf, settings, bitmap_settings, bitmap)) {
    Send(new ChromeUtilityHostMsg_RenderPDFPagesToPWGRaster_Succeeded());
  } else {
    Send(new ChromeUtilityHostMsg_RenderPDFPagesToPWGRaster_Failed());
  }
  ReleaseProcessIfNeeded();
}

#if defined(OS_WIN)
bool ChromeContentUtilityClient::RenderPDFToWinMetafile(
    base::PlatformFile pdf_file,
    const base::FilePath& metafile_path,
    const printing::PdfRenderSettings& settings,
    const std::vector<printing::PageRange>& page_ranges,
    int* highest_rendered_page_number,
    double* scale_factor) {
  *highest_rendered_page_number = -1;
  *scale_factor = 1.0;
  base::win::ScopedHandle file(pdf_file);

  if (!g_pdf_lib.Get().IsValid())
    return false;

  // TODO(sanjeevr): Add a method to the PDF DLL that takes in a file handle
  // and a page range array. That way we don't need to read the entire PDF into
  // memory.
  DWORD length = ::GetFileSize(file, NULL);
  if (length == INVALID_FILE_SIZE)
    return false;

  std::vector<uint8> buffer;
  buffer.resize(length);
  DWORD bytes_read = 0;
  if (!ReadFile(pdf_file, &buffer.front(), length, &bytes_read, NULL) ||
      (bytes_read != length)) {
    return false;
  }

  int total_page_count = 0;
  if (!g_pdf_lib.Get().GetPDFDocInfo(&buffer.front(), buffer.size(),
                                     &total_page_count, NULL)) {
    return false;
  }

  printing::Emf metafile;
  metafile.InitToFile(metafile_path);
  // We need to scale down DC to fit an entire page into DC available area.
  // Current metafile is based on screen DC and have current screen size.
  // Writing outside of those boundaries will result in the cut-off output.
  // On metafiles (this is the case here), scaling down will still record
  // original coordinates and we'll be able to print in full resolution.
  // Before playback we'll need to counter the scaling up that will happen
  // in the service (print_system_win.cc).
  *scale_factor = gfx::CalculatePageScale(metafile.context(),
                                          settings.area().right(),
                                          settings.area().bottom());
  gfx::ScaleDC(metafile.context(), *scale_factor);

  bool ret = false;
  std::vector<printing::PageRange>::const_iterator iter;
  for (iter = page_ranges.begin(); iter != page_ranges.end(); ++iter) {
    for (int page_number = iter->from; page_number <= iter->to; ++page_number) {
      if (page_number >= total_page_count)
        break;
      // The underlying metafile is of type Emf and ignores the arguments passed
      // to StartPage.
      metafile.StartPage(gfx::Size(), gfx::Rect(), 1);
      if (g_pdf_lib.Get().RenderPDFPageToDC(
              &buffer.front(), buffer.size(), page_number, metafile.context(),
              settings.dpi(), settings.dpi(), settings.area().x(),
              settings.area().y(), settings.area().width(),
              settings.area().height(), true, false, true, true,
              settings.autorotate())) {
        if (*highest_rendered_page_number < page_number)
          *highest_rendered_page_number = page_number;
        ret = true;
      }
      metafile.FinishPage();
    }
  }
  metafile.FinishDocument();
  return ret;
}
#endif  // defined(OS_WIN)

bool ChromeContentUtilityClient::RenderPDFPagesToPWGRaster(
    base::PlatformFile pdf_file,
    const printing::PdfRenderSettings& settings,
    const printing::PwgRasterSettings& bitmap_settings,
    base::PlatformFile bitmap_file) {
  bool autoupdate = true;
  if (!g_pdf_lib.Get().IsValid())
    return false;

  base::PlatformFileInfo info;
  if (!base::GetPlatformFileInfo(pdf_file, &info) || info.size <= 0)
    return false;

  std::string data(info.size, 0);
  int data_size = base::ReadPlatformFile(pdf_file, 0, &data[0], data.size());
  if (data_size != static_cast<int>(data.size()))
    return false;

  int total_page_count = 0;
  if (!g_pdf_lib.Get().GetPDFDocInfo(data.data(), data.size(),
                                     &total_page_count, NULL)) {
    return false;
  }

  cloud_print::PwgEncoder encoder;
  std::string pwg_header;
  encoder.EncodeDocumentHeader(&pwg_header);
  int bytes_written = base::WritePlatformFileAtCurrentPos(bitmap_file,
                                                          pwg_header.data(),
                                                          pwg_header.size());
  if (bytes_written != static_cast<int>(pwg_header.size()))
    return false;

  cloud_print::BitmapImage image(settings.area().size(),
                                 cloud_print::BitmapImage::BGRA);
  for (int i = 0; i < total_page_count; ++i) {
    int page_number = i;

    if (bitmap_settings.reverse_page_order) {
      page_number = total_page_count - 1 - page_number;
    }

    bool rotate = false;

    // Transform odd pages.
    if (page_number % 2) {
      rotate =
          (bitmap_settings.odd_page_transform != printing::TRANSFORM_NORMAL);
    }

    if (!g_pdf_lib.Get().RenderPDFPageToBitmap(data.data(),
                                               data.size(),
                                               page_number,
                                               image.pixel_data(),
                                               image.size().width(),
                                               image.size().height(),
                                               settings.dpi(),
                                               settings.dpi(),
                                               autoupdate)) {
      return false;
    }
    std::string pwg_page;
    if (!encoder.EncodePage(
            image, settings.dpi(), total_page_count, &pwg_page, rotate))
      return false;
    bytes_written = base::WritePlatformFileAtCurrentPos(bitmap_file,
                                                        pwg_page.data(),
                                                        pwg_page.size());
    if (bytes_written != static_cast<int>(pwg_page.size()))
      return false;
  }
  return true;
}

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

void ChromeContentUtilityClient::OnGetPrinterCapsAndDefaults(
    const std::string& printer_name) {
#if defined(ENABLE_FULL_PRINTING)
  scoped_refptr<printing::PrintBackend> print_backend =
      printing::PrintBackend::CreateInstance(NULL);
  printing::PrinterCapsAndDefaults printer_info;

  crash_keys::ScopedPrinterInfo crash_key(
      print_backend->GetPrinterDriverInfo(printer_name));

  if (print_backend->GetPrinterCapsAndDefaults(printer_name, &printer_info)) {
    Send(new ChromeUtilityHostMsg_GetPrinterCapsAndDefaults_Succeeded(
        printer_name, printer_info));
  } else  // NOLINT
#endif
  {
    Send(new ChromeUtilityHostMsg_GetPrinterCapsAndDefaults_Failed(
        printer_name));
  }
  ReleaseProcessIfNeeded();
}

void ChromeContentUtilityClient::OnGetPrinterSemanticCapsAndDefaults(
    const std::string& printer_name) {
#if defined(ENABLE_FULL_PRINTING)
  scoped_refptr<printing::PrintBackend> print_backend =
      printing::PrintBackend::CreateInstance(NULL);
  printing::PrinterSemanticCapsAndDefaults printer_info;

  crash_keys::ScopedPrinterInfo crash_key(
      print_backend->GetPrinterDriverInfo(printer_name));

  if (print_backend->GetPrinterSemanticCapsAndDefaults(printer_name,
                                                       &printer_info)) {
    Send(new ChromeUtilityHostMsg_GetPrinterSemanticCapsAndDefaults_Succeeded(
        printer_name, printer_info));
  } else  // NOLINT
#endif
  {
    Send(new ChromeUtilityHostMsg_GetPrinterSemanticCapsAndDefaults_Failed(
        printer_name));
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
      IPC::PlatformFileForTransitToPlatformFile(zip_file), &results);
  Send(new ChromeUtilityHostMsg_AnalyzeZipFileForDownloadProtection_Finished(
      results));
  ReleaseProcessIfNeeded();
}

#if !defined(OS_ANDROID) && !defined(OS_IOS)
void ChromeContentUtilityClient::OnCheckMediaFile(
    int64 milliseconds_of_decoding,
    const IPC::PlatformFileForTransit& media_file) {
  media::MediaFileChecker checker(
      base::File(IPC::PlatformFileForTransitToPlatformFile(media_file)));
  const bool check_success = checker.Start(
      base::TimeDelta::FromMilliseconds(milliseconds_of_decoding));
  Send(new ChromeUtilityHostMsg_CheckMediaFile_Finished(check_success));
  ReleaseProcessIfNeeded();
}

void ChromeContentUtilityClient::OnParseMediaMetadata(
    const std::string& mime_type,
    int64 total_size) {
  // Only one IPCDataSource may be created and added to the list of handlers.
  metadata::IPCDataSource* source = new metadata::IPCDataSource(total_size);
  handlers_.push_back(source);

  metadata::MediaMetadataParser* parser =
      new metadata::MediaMetadataParser(source, mime_type);
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
  base::PlatformFile file =
      IPC::PlatformFileForTransitToPlatformFile(iphoto_library_file);
  bool result = parser.Parse(iapps::ReadPlatformFileAsString(file));
  Send(new ChromeUtilityHostMsg_GotIPhotoLibrary(result, parser.library()));
  ReleaseProcessIfNeeded();
}
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN) || defined(OS_MACOSX)
void ChromeContentUtilityClient::OnParseITunesLibraryXmlFile(
    const IPC::PlatformFileForTransit& itunes_library_file) {
  itunes::ITunesLibraryParser parser;
  base::PlatformFile file =
      IPC::PlatformFileForTransitToPlatformFile(itunes_library_file);
  bool result = parser.Parse(iapps::ReadPlatformFileAsString(file));
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

}  // namespace chrome
