// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/chrome_content_utility_client.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop_proxy.h"
#include "base/threading/thread.h"
#include "chrome/browser/importer/importer.h"
#include "chrome/browser/importer/profile_import_process_messages.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_utility_messages.h"
#include "chrome/common/extensions/api/extension_action/browser_action_handler.h"
#include "chrome/common/extensions/api/extension_action/page_action_handler.h"
#include "chrome/common/extensions/api/i18n/default_locale_handler.h"
#include "chrome/common/extensions/api/icons/icons_handler.h"
#include "chrome/common/extensions/api/plugins/plugins_handler.h"
#include "chrome/common/extensions/api/themes/theme_handler.h"
#include "chrome/common/extensions/background_info.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_l10n_util.h"
#include "chrome/common/extensions/incognito_handler.h"
#include "chrome/common/extensions/manifest.h"
#include "chrome/common/extensions/permissions/chrome_api_permissions.h"
#include "chrome/common/extensions/unpacker.h"
#include "chrome/common/extensions/update_manifest.h"
#include "chrome/common/safe_browsing/zip_analyzer.h"
#include "chrome/common/web_resource/web_resource_unpacker.h"
#include "chrome/common/zip.h"
#include "chrome/utility/profile_import_handler.h"
#include "content/public/utility/utility_thread.h"
#include "printing/backend/print_backend.h"
#include "printing/page_range.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/rect.h"
#include "webkit/glue/image_decoder.h"

#if defined(OS_WIN)
#include "base/path_service.h"
#include "base/win/iat_patch_function.h"
#include "base/win/scoped_handle.h"
#include "content/public/common/content_switches.h"
#include "printing/emf_win.h"
#include "ui/gfx/gdi_util.h"
#endif  // defined(OS_WIN)

namespace {

// Explicitly register all ManifestHandlers needed in the utility process.
void RegisterExtensionManifestHandlers() {
  (new extensions::BackgroundManifestHandler)->Register();
  (new extensions::BrowserActionHandler)->Register();
  (new extensions::DefaultLocaleHandler)->Register();
  (new extensions::IconsHandler)->Register();
  (new extensions::PageActionHandler)->Register();
  (new extensions::ThemeHandler)->Register();
  (new extensions::PluginsHandler)->Register();
  (new extensions::IncognitoHandler)->Register();
}

}  // namespace

namespace chrome {

ChromeContentUtilityClient::ChromeContentUtilityClient() {
#if !defined(OS_ANDROID)
  import_handler_.reset(new ProfileImportHandler());
#endif
}

ChromeContentUtilityClient::~ChromeContentUtilityClient() {
}

void ChromeContentUtilityClient::UtilityThreadStarted() {
#if defined(OS_WIN)
  // Load the pdf plugin before the sandbox is turned on. This is for Windows
  // only because we need this DLL only on Windows.
  base::FilePath pdf;
  if (PathService::Get(chrome::FILE_PDF_PLUGIN, &pdf) &&
      file_util::PathExists(pdf)) {
    bool rv = !!LoadLibrary(pdf.value().c_str());
    DCHECK(rv) << "Couldn't load PDF plugin";
  }
#endif

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  std::string lang = command_line->GetSwitchValueASCII(switches::kLang);
  if (!lang.empty())
    extension_l10n_util::SetProcessLocale(lang);
}

bool ChromeContentUtilityClient::OnMessageReceived(
    const IPC::Message& message) {
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
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_RobustJPEGDecodeImage,
                        OnRobustJPEGDecodeImage)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_ParseJSON, OnParseJSON)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_GetPrinterCapsAndDefaults,
                        OnGetPrinterCapsAndDefaults)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_StartupPing, OnStartupPing)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_AnalyzeZipFileForDownloadProtection,
                        OnAnalyzeZipFileForDownloadProtection)

#if defined(OS_CHROMEOS)
    IPC_MESSAGE_HANDLER(ChromeUtilityMsg_CreateZipFile, OnCreateZipFile)
#endif  // defined(OS_CHROMEOS)

    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

#if !defined(OS_ANDROID)
  if (!handled)
    handled = import_handler_->OnMessageReceived(message);
#endif

  return handled;
}

bool ChromeContentUtilityClient::Send(IPC::Message* message) {
  return content::UtilityThread::Get()->Send(message);
}

void ChromeContentUtilityClient::OnUnpackExtension(
    const base::FilePath& extension_path,
    const std::string& extension_id,
    int location,
    int creation_flags) {
  CHECK(location > extensions::Manifest::INVALID_LOCATION);
  CHECK(location < extensions::Manifest::NUM_LOCATIONS);
  extensions::ChromeAPIPermissions permissions;
  extensions::PermissionsInfo::GetInstance()->InitializeWithDelegate(
      permissions);
  RegisterExtensionManifestHandlers();
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

  content::UtilityThread::Get()->ReleaseProcessIfNeeded();
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

  content::UtilityThread::Get()->ReleaseProcessIfNeeded();
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
  content::UtilityThread::Get()->ReleaseProcessIfNeeded();
}

void ChromeContentUtilityClient::OnDecodeImage(
    const std::vector<unsigned char>& encoded_data) {
  webkit_glue::ImageDecoder decoder;
  const SkBitmap& decoded_image = decoder.Decode(&encoded_data[0],
                                                 encoded_data.size());
  if (decoded_image.empty()) {
    Send(new ChromeUtilityHostMsg_DecodeImage_Failed());
  } else {
    Send(new ChromeUtilityHostMsg_DecodeImage_Succeeded(decoded_image));
  }
  content::UtilityThread::Get()->ReleaseProcessIfNeeded();
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
  content::UtilityThread::Get()->ReleaseProcessIfNeeded();
}
#endif  // defined(OS_CHROMEOS)

void ChromeContentUtilityClient::OnRenderPDFPagesToMetafile(
    base::PlatformFile pdf_file,
    const base::FilePath& metafile_path,
    const printing::PdfRenderSettings& pdf_render_settings,
    const std::vector<printing::PageRange>& page_ranges) {
  bool succeeded = false;
#if defined(OS_WIN)
  int highest_rendered_page_number = 0;
  double scale_factor = 1.0;
  succeeded = RenderPDFToWinMetafile(pdf_file,
                                     metafile_path,
                                     pdf_render_settings.area(),
                                     pdf_render_settings.dpi(),
                                     pdf_render_settings.autorotate(),
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
  content::UtilityThread::Get()->ReleaseProcessIfNeeded();
}

#if defined(OS_WIN)
// Exported by pdf.dll
typedef bool (*RenderPDFPageToDCProc)(
    const unsigned char* pdf_buffer, int buffer_size, int page_number, HDC dc,
    int dpi_x, int dpi_y, int bounds_origin_x, int bounds_origin_y,
    int bounds_width, int bounds_height, bool fit_to_bounds,
    bool stretch_to_bounds, bool keep_aspect_ratio, bool center_in_bounds,
    bool autorotate);

typedef bool (*GetPDFDocInfoProc)(const unsigned char* pdf_buffer,
                                  int buffer_size, int* page_count,
                                  double* max_page_width);

// The 2 below IAT patch functions are almost identical to the code in
// render_process_impl.cc. This is needed to work around specific Windows APIs
// used by the Chrome PDF plugin that will fail in the sandbox.
static base::win::IATPatchFunction g_iat_patch_createdca;
HDC WINAPI UtilityProcess_CreateDCAPatch(LPCSTR driver_name,
                                         LPCSTR device_name,
                                         LPCSTR output,
                                         const DEVMODEA* init_data) {
  if (driver_name &&
      (std::string("DISPLAY") == std::string(driver_name)))
  // CreateDC fails behind the sandbox, but not CreateCompatibleDC.
    return CreateCompatibleDC(NULL);

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
      std::vector<char> font_data;
      content::UtilityThread::Get()->PreCacheFont(logfont);
      rv = GetFontData(hdc, table, offset, buffer, length);
      content::UtilityThread::Get()->ReleaseCachedFonts();
    }
  }
  return rv;
}

bool ChromeContentUtilityClient::RenderPDFToWinMetafile(
    base::PlatformFile pdf_file,
    const base::FilePath& metafile_path,
    const gfx::Rect& render_area,
    int render_dpi,
    bool autorotate,
    const std::vector<printing::PageRange>& page_ranges,
    int* highest_rendered_page_number,
    double* scale_factor) {
  *highest_rendered_page_number = -1;
  *scale_factor = 1.0;
  base::win::ScopedHandle file(pdf_file);
  base::FilePath pdf_module_path;
  PathService::Get(chrome::FILE_PDF_PLUGIN, &pdf_module_path);
  HMODULE pdf_module = GetModuleHandle(pdf_module_path.value().c_str());
  if (!pdf_module)
    return false;

  RenderPDFPageToDCProc render_proc =
      reinterpret_cast<RenderPDFPageToDCProc>(
          GetProcAddress(pdf_module, "RenderPDFPageToDC"));
  if (!render_proc)
    return false;

  GetPDFDocInfoProc get_info_proc = reinterpret_cast<GetPDFDocInfoProc>(
          GetProcAddress(pdf_module, "GetPDFDocInfo"));
  if (!get_info_proc)
    return false;

  // Patch the IAT for handling specific APIs known to fail in the sandbox.
  if (!g_iat_patch_createdca.is_patched())
    g_iat_patch_createdca.Patch(pdf_module_path.value().c_str(),
                                "gdi32.dll", "CreateDCA",
                                UtilityProcess_CreateDCAPatch);

  if (!g_iat_patch_get_font_data.is_patched())
    g_iat_patch_get_font_data.Patch(pdf_module_path.value().c_str(),
                                    "gdi32.dll", "GetFontData",
                                    UtilityProcess_GetFontDataPatch);

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
      (bytes_read != length))
    return false;

  int total_page_count = 0;
  if (!get_info_proc(&buffer.front(), buffer.size(), &total_page_count, NULL))
    return false;

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
                                          render_area.right(),
                                          render_area.bottom());
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
      if (render_proc(&buffer.front(), buffer.size(), page_number,
                      metafile.context(), render_dpi, render_dpi,
                      render_area.x(), render_area.y(), render_area.width(),
                      render_area.height(), true, false, true, true,
                      autorotate))
        if (*highest_rendered_page_number < page_number)
          *highest_rendered_page_number = page_number;
        ret = true;
      metafile.FinishPage();
    }
  }
  metafile.FinishDocument();
  return ret;
}
#endif  // defined(OS_WIN)

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
  content::UtilityThread::Get()->ReleaseProcessIfNeeded();
}

void ChromeContentUtilityClient::OnParseJSON(const std::string& json) {
  int error_code;
  std::string error;
  Value* value = base::JSONReader::ReadAndReturnError(
      json, base::JSON_PARSE_RFC, &error_code, &error);
  if (value) {
    ListValue wrapper;
    wrapper.Append(value);
    Send(new ChromeUtilityHostMsg_ParseJSON_Succeeded(wrapper));
  } else {
    Send(new ChromeUtilityHostMsg_ParseJSON_Failed(error));
  }
  content::UtilityThread::Get()->ReleaseProcessIfNeeded();
}

void ChromeContentUtilityClient::OnGetPrinterCapsAndDefaults(
    const std::string& printer_name) {
#if defined(ENABLE_PRINTING)
  scoped_refptr<printing::PrintBackend> print_backend =
      printing::PrintBackend::CreateInstance(NULL);
  printing::PrinterCapsAndDefaults printer_info;

  child_process_logging::ScopedPrinterInfoSetter prn_info(
      print_backend->GetPrinterDriverInfo(printer_name));

  if (print_backend->GetPrinterCapsAndDefaults(printer_name, &printer_info)) {
    Send(new ChromeUtilityHostMsg_GetPrinterCapsAndDefaults_Succeeded(
        printer_name, printer_info));
  } else
#endif
  {
    Send(new ChromeUtilityHostMsg_GetPrinterCapsAndDefaults_Failed(
        printer_name));
  }
  content::UtilityThread::Get()->ReleaseProcessIfNeeded();
}

void ChromeContentUtilityClient::OnStartupPing() {
  Send(new ChromeUtilityHostMsg_ProcessStarted);
  // Don't release the process, we assume further messages are on the way.
}

void ChromeContentUtilityClient::OnAnalyzeZipFileForDownloadProtection(
    IPC::PlatformFileForTransit zip_file) {
  safe_browsing::zip_analyzer::Results results;
  safe_browsing::zip_analyzer::AnalyzeZipFile(
      IPC::PlatformFileForTransitToPlatformFile(zip_file), &results);
  Send(new ChromeUtilityHostMsg_AnalyzeZipFileForDownloadProtection_Finished(
      results));
  content::UtilityThread::Get()->ReleaseProcessIfNeeded();
}

}  // namespace chrome
