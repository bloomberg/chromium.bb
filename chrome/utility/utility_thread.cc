// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/utility_thread.h"

#include <vector>

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_unpacker.h"
#include "chrome/common/extensions/update_manifest.h"
#include "chrome/common/indexed_db_key.h"
#include "chrome/common/serialized_script_value.h"
#include "chrome/common/utility_messages.h"
#include "chrome/common/web_resource/web_resource_unpacker.h"
#include "content/common/child_process.h"
#include "printing/page_range.h"
#include "printing/units.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSerializedScriptValue.h"
#include "ui/gfx/rect.h"
#include "webkit/glue/idb_bindings.h"
#include "webkit/glue/image_decoder.h"

#if defined(OS_WIN)
#include "app/win/iat_patch_function.h"
#include "base/scoped_ptr.h"
#include "base/win/scoped_handle.h"
#include "printing/native_metafile_factory.h"
#include "printing/native_metafile.h"
#endif

namespace {

template<typename SRC, typename DEST>
void ConvertVector(const SRC& src, DEST* dest) {
  dest->reserve(src.size());
  for (typename SRC::const_iterator i = src.begin(); i != src.end(); ++i)
    dest->push_back(typename DEST::value_type(*i));
}

}  // namespace


UtilityThread::UtilityThread()
    : batch_mode_(false) {
  ChildProcess::current()->AddRefProcess();
}

UtilityThread::~UtilityThread() {
}

bool UtilityThread::OnControlMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(UtilityThread, msg)
    IPC_MESSAGE_HANDLER(UtilityMsg_UnpackExtension, OnUnpackExtension)
    IPC_MESSAGE_HANDLER(UtilityMsg_UnpackWebResource, OnUnpackWebResource)
    IPC_MESSAGE_HANDLER(UtilityMsg_ParseUpdateManifest, OnParseUpdateManifest)
    IPC_MESSAGE_HANDLER(UtilityMsg_DecodeImage, OnDecodeImage)
    IPC_MESSAGE_HANDLER(UtilityMsg_RenderPDFPagesToMetafile,
                        OnRenderPDFPagesToMetafile)
    IPC_MESSAGE_HANDLER(UtilityMsg_IDBKeysFromValuesAndKeyPath,
                        OnIDBKeysFromValuesAndKeyPath)
    IPC_MESSAGE_HANDLER(UtilityMsg_InjectIDBKey,
                        OnInjectIDBKey)
    IPC_MESSAGE_HANDLER(UtilityMsg_BatchMode_Started, OnBatchModeStarted)
    IPC_MESSAGE_HANDLER(UtilityMsg_BatchMode_Finished, OnBatchModeFinished)
    IPC_MESSAGE_HANDLER(UtilityMsg_GetPrinterCapsAndDefaults,
                        OnGetPrinterCapsAndDefaults)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void UtilityThread::OnUnpackExtension(const FilePath& extension_path) {
  ExtensionUnpacker unpacker(extension_path);
  if (unpacker.Run() && unpacker.DumpImagesToFile() &&
      unpacker.DumpMessageCatalogsToFile()) {
    Send(new UtilityHostMsg_UnpackExtension_Succeeded(
       *unpacker.parsed_manifest()));
  } else {
    Send(new UtilityHostMsg_UnpackExtension_Failed(unpacker.error_message()));
  }

  ReleaseProcessIfNeeded();
}

void UtilityThread::OnUnpackWebResource(const std::string& resource_data) {
  // Parse json data.
  // TODO(mrc): Add the possibility of a template that controls parsing, and
  // the ability to download and verify images.
  WebResourceUnpacker unpacker(resource_data);
  if (unpacker.Run()) {
    Send(new UtilityHostMsg_UnpackWebResource_Succeeded(
        *unpacker.parsed_json()));
  } else {
    Send(new UtilityHostMsg_UnpackWebResource_Failed(
        unpacker.error_message()));
  }

  ReleaseProcessIfNeeded();
}

void UtilityThread::OnParseUpdateManifest(const std::string& xml) {
  UpdateManifest manifest;
  if (!manifest.Parse(xml)) {
    Send(new UtilityHostMsg_ParseUpdateManifest_Failed(manifest.errors()));
  } else {
    Send(new UtilityHostMsg_ParseUpdateManifest_Succeeded(manifest.results()));
  }
  ReleaseProcessIfNeeded();
}

void UtilityThread::OnDecodeImage(
    const std::vector<unsigned char>& encoded_data) {
  webkit_glue::ImageDecoder decoder;
  const SkBitmap& decoded_image = decoder.Decode(&encoded_data[0],
                                                 encoded_data.size());
  if (decoded_image.empty()) {
    Send(new UtilityHostMsg_DecodeImage_Failed());
  } else {
    Send(new UtilityHostMsg_DecodeImage_Succeeded(decoded_image));
  }
  ReleaseProcessIfNeeded();
}


void UtilityThread::OnRenderPDFPagesToMetafile(
    base::PlatformFile pdf_file,
    const FilePath& metafile_path,
    const gfx::Rect& render_area,
    int render_dpi,
    const std::vector<printing::PageRange>& page_ranges) {
  bool succeeded = false;
#if defined(OS_WIN)
  int highest_rendered_page_number = 0;
  succeeded = RenderPDFToWinMetafile(pdf_file,
                                     metafile_path,
                                     render_area,
                                     render_dpi,
                                     page_ranges,
                                     &highest_rendered_page_number);
  if (succeeded) {
    Send(new UtilityHostMsg_RenderPDFPagesToMetafile_Succeeded(
        highest_rendered_page_number));
  }
#endif  // defined(OS_WIN)
  if (!succeeded) {
    Send(new UtilityHostMsg_RenderPDFPagesToMetafile_Failed());
  }
  ReleaseProcessIfNeeded();
}

#if defined(OS_WIN)
// Exported by pdf.dll
typedef bool (*RenderPDFPageToDCProc)(
    const unsigned char* pdf_buffer, int buffer_size, int page_number, HDC dc,
    int dpi_x, int dpi_y, int bounds_origin_x, int bounds_origin_y,
    int bounds_width, int bounds_height, bool fit_to_bounds,
    bool stretch_to_bounds, bool keep_aspect_ratio, bool center_in_bounds);

typedef bool (*GetPDFDocInfoProc)(const unsigned char* pdf_buffer,
                                  int buffer_size, int* page_count,
                                  double* max_page_width);

// The 2 below IAT patch functions are almost identical to the code in
// render_process_impl.cc. This is needed to work around specific Windows APIs
// used by the Chrome PDF plugin that will fail in the sandbox.
static app::win::IATPatchFunction g_iat_patch_createdca;
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

static app::win::IATPatchFunction g_iat_patch_get_font_data;
DWORD WINAPI UtilityProcess_GetFontDataPatch(
    HDC hdc, DWORD table, DWORD offset, LPVOID buffer, DWORD length) {
  int rv = GetFontData(hdc, table, offset, buffer, length);
  if (rv == GDI_ERROR && hdc) {
    HFONT font = static_cast<HFONT>(GetCurrentObject(hdc, OBJ_FONT));

    LOGFONT logfont;
    if (GetObject(font, sizeof(LOGFONT), &logfont)) {
      std::vector<char> font_data;
      if (UtilityThread::current()->Send(
              new UtilityHostMsg_PreCacheFont(logfont)))
        rv = GetFontData(hdc, table, offset, buffer, length);
    }
  }
  return rv;
}

bool UtilityThread::RenderPDFToWinMetafile(
    base::PlatformFile pdf_file,
    const FilePath& metafile_path,
    const gfx::Rect& render_area,
    int render_dpi,
    const std::vector<printing::PageRange>& page_ranges,
    int* highest_rendered_page_number) {
  *highest_rendered_page_number = -1;
  base::win::ScopedHandle file(pdf_file);
  FilePath pdf_module_path;
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

  scoped_ptr<printing::NativeMetafile> metafile(
      printing::NativeMetafileFactory::CreateMetafile());
  metafile->CreateFileBackedDc(NULL, NULL, metafile_path);
  // Since we created the metafile using the screen DPI (but we actually want
  // the PDF DLL to print using the passed in render_dpi, we apply the following
  // transformation.
  SetGraphicsMode(metafile->context(), GM_ADVANCED);
  XFORM xform = {0};
  int screen_dpi = GetDeviceCaps(GetDC(NULL), LOGPIXELSX);
  xform.eM11 = xform.eM22 =
      static_cast<float>(screen_dpi) / static_cast<float>(render_dpi);
  ModifyWorldTransform(metafile->context(), &xform, MWT_LEFTMULTIPLY);

  bool ret = false;
  std::vector<printing::PageRange>::const_iterator iter;
  for (iter = page_ranges.begin(); iter != page_ranges.end(); ++iter) {
    for (int page_number = iter->from; page_number <= iter->to; ++page_number) {
      if (page_number >= total_page_count)
        break;
      metafile->StartPage();
      if (render_proc(&buffer.front(), buffer.size(), page_number,
                      metafile->context(), render_dpi, render_dpi,
                      render_area.x(), render_area.y(), render_area.width(),
                      render_area.height(), true, false, true, true))
        if (*highest_rendered_page_number < page_number)
          *highest_rendered_page_number = page_number;
        ret = true;
      metafile->FinishPage();
    }
  }
  metafile->Close();
  return ret;
}
#endif  // defined(OS_WIN)

void UtilityThread::OnIDBKeysFromValuesAndKeyPath(
    int id,
    const std::vector<SerializedScriptValue>& serialized_script_values,
    const string16& idb_key_path) {
  std::vector<WebKit::WebSerializedScriptValue> web_values;
  ConvertVector(serialized_script_values, &web_values);
  std::vector<WebKit::WebIDBKey> web_keys;
  bool error = webkit_glue::IDBKeysFromValuesAndKeyPath(
      web_values, idb_key_path, &web_keys);
  if (error) {
    Send(new UtilityHostMsg_IDBKeysFromValuesAndKeyPath_Failed(id));
    return;
  }
  std::vector<IndexedDBKey> keys;
  ConvertVector(web_keys, &keys);
  Send(new UtilityHostMsg_IDBKeysFromValuesAndKeyPath_Succeeded(id, keys));
  ReleaseProcessIfNeeded();
}

void UtilityThread::OnInjectIDBKey(const IndexedDBKey& key,
                                   const SerializedScriptValue& value,
                                   const string16& key_path) {
  SerializedScriptValue new_value(webkit_glue::InjectIDBKey(key, value,
                                                              key_path));
  Send(new UtilityHostMsg_InjectIDBKey_Finished(new_value));
  ReleaseProcessIfNeeded();
}

void UtilityThread::OnBatchModeStarted() {
  batch_mode_ = true;
}

void UtilityThread::OnBatchModeFinished() {
  ChildProcess::current()->ReleaseProcess();
}

void UtilityThread::OnGetPrinterCapsAndDefaults(
    const std::string& printer_name) {
  scoped_refptr<printing::PrintBackend> print_backend =
      printing::PrintBackend::CreateInstance(NULL);
  printing::PrinterCapsAndDefaults printer_info;
  if (print_backend->GetPrinterCapsAndDefaults(printer_name, &printer_info)) {
    Send(new UtilityHostMsg_GetPrinterCapsAndDefaults_Succeeded(printer_name,
                                                                printer_info));
  } else {
    Send(new UtilityHostMsg_GetPrinterCapsAndDefaults_Failed(printer_name));
  }
  ReleaseProcessIfNeeded();
}

void UtilityThread::ReleaseProcessIfNeeded() {
  if (!batch_mode_)
    ChildProcess::current()->ReleaseProcess();
}
