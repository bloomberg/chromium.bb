// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, so no include guard.

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/platform_file.h"
#include "base/values.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/update_manifest.h"
#include "chrome/common/safe_browsing/zip_analyzer.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"
#include "printing/backend/print_backend.h"
#include "printing/page_range.h"
#include "printing/pdf_render_settings.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/rect.h"

#define IPC_MESSAGE_START ChromeUtilityMsgStart

IPC_STRUCT_TRAITS_BEGIN(printing::PageRange)
  IPC_STRUCT_TRAITS_MEMBER(from)
  IPC_STRUCT_TRAITS_MEMBER(to)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(printing::PrinterCapsAndDefaults)
  IPC_STRUCT_TRAITS_MEMBER(printer_capabilities)
  IPC_STRUCT_TRAITS_MEMBER(caps_mime_type)
  IPC_STRUCT_TRAITS_MEMBER(printer_defaults)
  IPC_STRUCT_TRAITS_MEMBER(defaults_mime_type)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(UpdateManifest::Result)
  IPC_STRUCT_TRAITS_MEMBER(extension_id)
  IPC_STRUCT_TRAITS_MEMBER(version)
  IPC_STRUCT_TRAITS_MEMBER(browser_min_version)
  IPC_STRUCT_TRAITS_MEMBER(package_hash)
  IPC_STRUCT_TRAITS_MEMBER(crx_url)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(UpdateManifest::Results)
  IPC_STRUCT_TRAITS_MEMBER(list)
  IPC_STRUCT_TRAITS_MEMBER(daystart_elapsed_seconds)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(safe_browsing::zip_analyzer::Results)
  IPC_STRUCT_TRAITS_MEMBER(success)
  IPC_STRUCT_TRAITS_MEMBER(has_executable)
  IPC_STRUCT_TRAITS_MEMBER(has_archive)
IPC_STRUCT_TRAITS_END()

//------------------------------------------------------------------------------
// Utility process messages:
// These are messages from the browser to the utility process.
// Tell the utility process to unpack the given extension file in its
// directory and verify that it is valid.
IPC_MESSAGE_CONTROL4(ChromeUtilityMsg_UnpackExtension,
                     base::FilePath /* extension_filename */,
                     std::string /* extension_id */,
                     int /* Manifest::Location */,
                     int /* InitFromValue flags */)

// Tell the utility process to parse the given JSON data and verify its
// validity.
IPC_MESSAGE_CONTROL1(ChromeUtilityMsg_UnpackWebResource,
                     std::string /* JSON data */)

// Tell the utility process to parse the given xml document.
IPC_MESSAGE_CONTROL1(ChromeUtilityMsg_ParseUpdateManifest,
                     std::string /* xml document contents */)

// Tell the utility process to decode the given image data.
IPC_MESSAGE_CONTROL1(ChromeUtilityMsg_DecodeImage,
                     std::vector<unsigned char>)  // encoded image contents

// Tell the utility process to decode the given image data, which is base64
// encoded.
IPC_MESSAGE_CONTROL1(ChromeUtilityMsg_DecodeImageBase64,
                     std::string)  // base64 encoded image contents

// Tell the utility process to render the given PDF into a metafile.
IPC_MESSAGE_CONTROL4(ChromeUtilityMsg_RenderPDFPagesToMetafile,
                     base::PlatformFile,       // PDF file
                     base::FilePath,           // Location for output metafile
                     printing::PdfRenderSettings,  // PDF render settitngs
                     std::vector<printing::PageRange>)

// Tell the utility process to decode the given JPEG image data with a robust
// libjpeg codec.
IPC_MESSAGE_CONTROL1(ChromeUtilityMsg_RobustJPEGDecodeImage,
                     std::vector<unsigned char>)  // encoded image contents

// Tell the utility process to parse a JSON string into a Value object.
IPC_MESSAGE_CONTROL1(ChromeUtilityMsg_ParseJSON,
                     std::string /* JSON to parse */)

// Tells the utility process to get capabilities and defaults for the specified
// printer. Used on Windows to isolate the service process from printer driver
// crashes by executing this in a separate process. This does not run in a
// sandbox.
IPC_MESSAGE_CONTROL1(ChromeUtilityMsg_GetPrinterCapsAndDefaults,
                     std::string /* printer name */)

#if defined(OS_CHROMEOS)
// Tell the utility process to create a zip file on the given list of files.
IPC_MESSAGE_CONTROL3(ChromeUtilityMsg_CreateZipFile,
                     base::FilePath /* src_dir */,
                     std::vector<base::FilePath> /* src_relative_paths */,
                     base::FileDescriptor /* dest_fd */)
#endif  // defined(OS_CHROMEOS)

// Requests the utility process to respond with a
// ChromeUtilityHostMsg_ProcessStarted message once it has started.  This may
// be used if the host process needs a handle to the running utility process.
IPC_MESSAGE_CONTROL0(ChromeUtilityMsg_StartupPing)

// Tells the utility process to analyze a zip file for malicious download
// protection.
IPC_MESSAGE_CONTROL1(ChromeUtilityMsg_AnalyzeZipFileForDownloadProtection,
                     IPC::PlatformFileForTransit /* zip_file */)

//------------------------------------------------------------------------------
// Utility process host messages:
// These are messages from the utility process to the browser.
// Reply when the utility process is done unpacking an extension.  |manifest|
// is the parsed manifest.json file.
// The unpacker should also have written out files containing the decoded
// images and message catalogs from the extension. See extensions::Unpacker for
// details.
IPC_MESSAGE_CONTROL1(ChromeUtilityHostMsg_UnpackExtension_Succeeded,
                     DictionaryValue /* manifest */)

// Reply when the utility process has failed while unpacking an extension.
// |error_message| is a user-displayable explanation of what went wrong.
IPC_MESSAGE_CONTROL1(ChromeUtilityHostMsg_UnpackExtension_Failed,
                     string16 /* error_message, if any */)

// Reply when the utility process is done unpacking and parsing JSON data
// from a web resource.
IPC_MESSAGE_CONTROL1(ChromeUtilityHostMsg_UnpackWebResource_Succeeded,
                     DictionaryValue /* json data */)

// Reply when the utility process has failed while unpacking and parsing a
// web resource.  |error_message| is a user-readable explanation of what
// went wrong.
IPC_MESSAGE_CONTROL1(ChromeUtilityHostMsg_UnpackWebResource_Failed,
                     std::string /* error_message, if any */)

// Reply when the utility process has succeeded in parsing an update manifest
// xml document.
IPC_MESSAGE_CONTROL1(ChromeUtilityHostMsg_ParseUpdateManifest_Succeeded,
                     UpdateManifest::Results /* updates */)

// Reply when an error occured parsing the update manifest. |error_message|
// is a description of what went wrong suitable for logging.
IPC_MESSAGE_CONTROL1(ChromeUtilityHostMsg_ParseUpdateManifest_Failed,
                     std::string /* error_message, if any */)

// Reply when the utility process has succeeded in decoding the image.
IPC_MESSAGE_CONTROL1(ChromeUtilityHostMsg_DecodeImage_Succeeded,
                     SkBitmap)  // decoded image

// Reply when an error occured decoding the image.
IPC_MESSAGE_CONTROL0(ChromeUtilityHostMsg_DecodeImage_Failed)

// Reply when the utility process has succeeded in rendering the PDF.
IPC_MESSAGE_CONTROL2(ChromeUtilityHostMsg_RenderPDFPagesToMetafile_Succeeded,
                     int,          // Highest rendered page number
                     double)       // Scale factor

// Reply when an error occured rendering the PDF.
IPC_MESSAGE_CONTROL0(ChromeUtilityHostMsg_RenderPDFPagesToMetafile_Failed)

// Reply when the utility process successfully parsed a JSON string.
//
// WARNING: The result can be of any Value subclass type, but we can't easily
// pass indeterminate value types by const object reference with our IPC macros,
// so we put the result Value into a ListValue. Handlers should examine the
// first (and only) element of the ListValue for the actual result.
IPC_MESSAGE_CONTROL1(ChromeUtilityHostMsg_ParseJSON_Succeeded,
                     ListValue)

// Reply when the utility process failed in parsing a JSON string.
IPC_MESSAGE_CONTROL1(ChromeUtilityHostMsg_ParseJSON_Failed,
                     std::string /* error message, if any*/)

#if defined(ENABLE_PRINTING)
// Reply when the utility process has succeeded in obtaining the printer
// capabilities and defaults.
IPC_MESSAGE_CONTROL2(ChromeUtilityHostMsg_GetPrinterCapsAndDefaults_Succeeded,
                     std::string /* printer name */,
                     printing::PrinterCapsAndDefaults)
#endif

// Reply when the utility process has failed to obtain the printer
// capabilities and defaults.
IPC_MESSAGE_CONTROL1(ChromeUtilityHostMsg_GetPrinterCapsAndDefaults_Failed,
                     std::string /* printer name */)

#if defined(OS_CHROMEOS)
// Reply when the utility process has succeeded in creating the zip file.
IPC_MESSAGE_CONTROL0(ChromeUtilityHostMsg_CreateZipFile_Succeeded)

// Reply when an error occured in creating the zip file.
IPC_MESSAGE_CONTROL0(ChromeUtilityHostMsg_CreateZipFile_Failed)
#endif  // defined(OS_CHROMEOS)

// Reply when the utility process has started.
IPC_MESSAGE_CONTROL0(ChromeUtilityHostMsg_ProcessStarted)

// Reply when a zip file has been analyzed for malicious download protection.
IPC_MESSAGE_CONTROL1(
    ChromeUtilityHostMsg_AnalyzeZipFileForDownloadProtection_Finished,
    safe_browsing::zip_analyzer::Results)
