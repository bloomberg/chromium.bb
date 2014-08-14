// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, so no include guard.

#if defined(OS_WIN)
#include <Windows.h>
#endif  // defined(OS_WIN)

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "base/tuple.h"
#include "base/values.h"
#include "chrome/common/safe_browsing/zip_analyzer.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/ipc/gfx_param_traits.h"

#define IPC_MESSAGE_START ChromeUtilityMsgStart

#ifndef CHROME_COMMON_CHROME_UTILITY_MESSAGES_H_
#define CHROME_COMMON_CHROME_UTILITY_MESSAGES_H_

typedef std::vector<Tuple2<SkBitmap, base::FilePath> > DecodedImages;

#endif  // CHROME_COMMON_CHROME_UTILITY_MESSAGES_H_

IPC_STRUCT_TRAITS_BEGIN(safe_browsing::zip_analyzer::Results)
  IPC_STRUCT_TRAITS_MEMBER(success)
  IPC_STRUCT_TRAITS_MEMBER(has_executable)
  IPC_STRUCT_TRAITS_MEMBER(has_archive)
IPC_STRUCT_TRAITS_END()

//------------------------------------------------------------------------------
// Utility process messages:
// These are messages from the browser to the utility process.

// Tell the utility process to parse the given JSON data and verify its
// validity.
IPC_MESSAGE_CONTROL1(ChromeUtilityMsg_UnpackWebResource,
                     std::string /* JSON data */)

// Tell the utility process to decode the given image data.
IPC_MESSAGE_CONTROL1(ChromeUtilityMsg_DecodeImage,
                     std::vector<unsigned char>)  // encoded image contents

// Tell the utility process to decode the given JPEG image data with a robust
// libjpeg codec.
IPC_MESSAGE_CONTROL1(ChromeUtilityMsg_RobustJPEGDecodeImage,
                     std::vector<unsigned char>)  // encoded image contents

// Tell the utility process to patch the given |input_file| using |patch_file|
// and place the output in |output_file|. The patch should use the bsdiff
// algorithm (Courgette's version).
IPC_MESSAGE_CONTROL3(ChromeUtilityMsg_PatchFileBsdiff,
                     base::FilePath /* input_file */,
                     base::FilePath /* patch_file */,
                     base::FilePath /* output_file */)

// Tell the utility process to patch the given |input_file| using |patch_file|
// and place the output in |output_file|. The patch should use the Courgette
// algorithm.
IPC_MESSAGE_CONTROL3(ChromeUtilityMsg_PatchFileCourgette,
                     base::FilePath /* input_file */,
                     base::FilePath /* patch_file */,
                     base::FilePath /* output_file */)

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

#if defined(FULL_SAFE_BROWSING)
// Tells the utility process to analyze a zip file for malicious download
// protection.
IPC_MESSAGE_CONTROL1(ChromeUtilityMsg_AnalyzeZipFileForDownloadProtection,
                     IPC::PlatformFileForTransit /* zip_file */)
#endif

#if defined(OS_WIN)
// A vector of filters, each being a Tuple2a display string (i.e. "Text Files")
// and a filter pattern (i.e. "*.txt")..
typedef std::vector<Tuple2<base::string16, base::string16> >
    GetOpenFileNameFilter;

// Instructs the utility process to invoke GetOpenFileName. |owner| is the
// parent of the modal dialog, |flags| are OFN_* flags. |filter| constrains the
// user's file choices. |initial_directory| and |filename| select the directory
// to be displayed and the file to be initially selected.
//
// Either ChromeUtilityHostMsg_GetOpenFileName_Failed or
// ChromeUtilityHostMsg_GetOpenFileName_Result will be returned when the
// operation completes whether due to error or user action.
IPC_MESSAGE_CONTROL5(ChromeUtilityMsg_GetOpenFileName,
                     HWND /* owner */,
                     DWORD /* flags */,
                     GetOpenFileNameFilter /* filter */,
                     base::FilePath /* initial_directory */,
                     base::FilePath /* filename */)
#endif  // defined(OS_WIN)

//------------------------------------------------------------------------------
// Utility process host messages:
// These are messages from the utility process to the browser.

// Reply when the utility process is done unpacking and parsing JSON data
// from a web resource.
IPC_MESSAGE_CONTROL1(ChromeUtilityHostMsg_UnpackWebResource_Succeeded,
                     base::DictionaryValue /* json data */)

// Reply when the utility process has failed while unpacking and parsing a
// web resource.  |error_message| is a user-readable explanation of what
// went wrong.
IPC_MESSAGE_CONTROL1(ChromeUtilityHostMsg_UnpackWebResource_Failed,
                     std::string /* error_message, if any */)

// Reply when the utility process has succeeded in decoding the image.
IPC_MESSAGE_CONTROL1(ChromeUtilityHostMsg_DecodeImage_Succeeded,
                     SkBitmap)  // decoded image

// Reply when an error occurred decoding the image.
IPC_MESSAGE_CONTROL0(ChromeUtilityHostMsg_DecodeImage_Failed)

// Reply when a file has been patched.
IPC_MESSAGE_CONTROL1(ChromeUtilityHostMsg_PatchFile_Finished, int /* result */)

#if defined(OS_CHROMEOS)
// Reply when the utility process has succeeded in creating the zip file.
IPC_MESSAGE_CONTROL0(ChromeUtilityHostMsg_CreateZipFile_Succeeded)

// Reply when an error occured in creating the zip file.
IPC_MESSAGE_CONTROL0(ChromeUtilityHostMsg_CreateZipFile_Failed)
#endif  // defined(OS_CHROMEOS)

// Reply when the utility process has started.
IPC_MESSAGE_CONTROL0(ChromeUtilityHostMsg_ProcessStarted)

#if defined(FULL_SAFE_BROWSING)
// Reply when a zip file has been analyzed for malicious download protection.
IPC_MESSAGE_CONTROL1(
    ChromeUtilityHostMsg_AnalyzeZipFileForDownloadProtection_Finished,
    safe_browsing::zip_analyzer::Results)
#endif

#if defined(OS_WIN)
IPC_MESSAGE_CONTROL0(ChromeUtilityHostMsg_GetOpenFileName_Failed)
IPC_MESSAGE_CONTROL2(ChromeUtilityHostMsg_GetOpenFileName_Result,
                     base::FilePath /* directory */,
                     std::vector<base::FilePath> /* filenames */)
#endif  // defined(OS_WIN)
