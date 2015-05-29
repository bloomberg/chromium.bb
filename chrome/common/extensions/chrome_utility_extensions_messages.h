// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, so no include guard.

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "chrome/common/media_galleries/iphoto_library.h"
#include "chrome/common/media_galleries/itunes_library.h"
#include "chrome/common/media_galleries/metadata_types.h"
#include "chrome/common/media_galleries/picasa_types.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"

#if !defined(ENABLE_EXTENSIONS)
#error "Extensions must be enabled"
#endif

#define IPC_MESSAGE_START ChromeUtilityExtensionsMsgStart

#if defined(OS_MACOSX)
IPC_STRUCT_TRAITS_BEGIN(iphoto::parser::Photo)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(location)
  IPC_STRUCT_TRAITS_MEMBER(original_location)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(iphoto::parser::Library)
  IPC_STRUCT_TRAITS_MEMBER(albums)
  IPC_STRUCT_TRAITS_MEMBER(all_photos)
IPC_STRUCT_TRAITS_END()
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN) || defined(OS_MACOSX)
IPC_STRUCT_TRAITS_BEGIN(itunes::parser::Track)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(location)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(picasa::AlbumInfo)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(timestamp)
  IPC_STRUCT_TRAITS_MEMBER(uid)
  IPC_STRUCT_TRAITS_MEMBER(path)
IPC_STRUCT_TRAITS_END()

// These files are opened read-only. Please see the constructor for
// picasa::AlbumTableFiles for details.
IPC_STRUCT_TRAITS_BEGIN(picasa::AlbumTableFilesForTransit)
  IPC_STRUCT_TRAITS_MEMBER(indicator_file)
  IPC_STRUCT_TRAITS_MEMBER(category_file)
  IPC_STRUCT_TRAITS_MEMBER(date_file)
  IPC_STRUCT_TRAITS_MEMBER(filename_file)
  IPC_STRUCT_TRAITS_MEMBER(name_file)
  IPC_STRUCT_TRAITS_MEMBER(token_file)
  IPC_STRUCT_TRAITS_MEMBER(uid_file)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(picasa::FolderINIContents)
  IPC_STRUCT_TRAITS_MEMBER(folder_path)
  IPC_STRUCT_TRAITS_MEMBER(ini_contents)
IPC_STRUCT_TRAITS_END()
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

IPC_STRUCT_TRAITS_BEGIN(metadata::AttachedImage)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(data)
IPC_STRUCT_TRAITS_END()

//------------------------------------------------------------------------------
// Utility process messages:
// These are messages from the browser to the utility process.


#if defined(OS_WIN)
// Tell the utility process to parse the iTunes preference XML file contents
// and return the path to the iTunes directory.
IPC_MESSAGE_CONTROL1(ChromeUtilityMsg_ParseITunesPrefXml,
                     std::string /* XML to parse */)
#endif  // defined(OS_WIN)

#if defined(OS_MACOSX)
// Tell the utility process to parse the iPhoto library XML file and
// return the parse result as well as the iPhoto library as an iphoto::Library.
IPC_MESSAGE_CONTROL1(ChromeUtilityMsg_ParseIPhotoLibraryXmlFile,
                     IPC::PlatformFileForTransit /* XML file to parse */)
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN) || defined(OS_MACOSX)
// Tell the utility process to parse the iTunes library XML file and
// return the parse result as well as the iTunes library as an itunes::Library.
IPC_MESSAGE_CONTROL1(ChromeUtilityMsg_ParseITunesLibraryXmlFile,
                     IPC::PlatformFileForTransit /* XML file to parse */)

// Tells the utility process to parse the Picasa PMP database and return a
// listing of the user's Picasa albums and folders, along with metadata.
IPC_MESSAGE_CONTROL1(ChromeUtilityMsg_ParsePicasaPMPDatabase,
                     picasa::AlbumTableFilesForTransit /* album_table_files */)

// Tells the utility process to index the Picasa user-created Album contents
// by parsing all the INI files in Picasa Folders.
IPC_MESSAGE_CONTROL2(ChromeUtilityMsg_IndexPicasaAlbumsContents,
                     picasa::AlbumUIDSet /* album_uids */,
                     std::vector<picasa::FolderINIContents> /* folders_inis */)
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

// Tell the utility process to attempt to validate the passed media file. The
// file will undergo basic sanity checks and will be decoded for up to
// |milliseconds_of_decoding| wall clock time. It is still not safe to decode
// the file in the browser process after this check.
IPC_MESSAGE_CONTROL2(ChromeUtilityMsg_CheckMediaFile,
                     int64 /* milliseconds_of_decoding */,
                     IPC::PlatformFileForTransit /* Media file to parse */)

IPC_MESSAGE_CONTROL3(ChromeUtilityMsg_ParseMediaMetadata,
                     std::string /* mime_type */,
                     int64 /* total_size */,
                     bool /* get_attached_images */)

IPC_MESSAGE_CONTROL2(ChromeUtilityMsg_RequestBlobBytes_Finished,
                     int64 /* request_id */,
                     std::string /* bytes */)

// Requests that the utility process write the contents of the source file to
// the removable drive listed in the target file. The target will be restricted
// to removable drives by the utility process.
IPC_MESSAGE_CONTROL2(ChromeUtilityMsg_ImageWriter_Write,
                     base::FilePath /* source file */,
                     base::FilePath /* target file */)

// Requests that the utility process verify that the contents of the source file
// was written to the target. As above the target will be restricted to
// removable drives by the utility process.
IPC_MESSAGE_CONTROL2(ChromeUtilityMsg_ImageWriter_Verify,
                     base::FilePath /* source file */,
                     base::FilePath /* target file */)

// Cancels a pending write or verify operation.
IPC_MESSAGE_CONTROL0(ChromeUtilityMsg_ImageWriter_Cancel)

//------------------------------------------------------------------------------
// Utility process host messages:
// These are messages from the utility process to the browser.

#if defined(OS_WIN)
// Reply after parsing the iTunes preferences XML file contents with either the
// path to the iTunes directory or an empty FilePath.
IPC_MESSAGE_CONTROL1(ChromeUtilityHostMsg_GotITunesDirectory,
                     base::FilePath /* Path to iTunes library */)
#endif  // defined(OS_WIN)

#if defined(OS_MACOSX)
// Reply after parsing the iPhoto library XML file with the parser result and
// an iphoto::Library data structure.
IPC_MESSAGE_CONTROL2(ChromeUtilityHostMsg_GotIPhotoLibrary,
                     bool /* Parser result */,
                     iphoto::parser::Library /* iPhoto library */)
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN) || defined(OS_MACOSX)
// Reply after parsing the iTunes library XML file with the parser result and
// an itunes::Library data structure.
IPC_MESSAGE_CONTROL2(ChromeUtilityHostMsg_GotITunesLibrary,
                     bool /* Parser result */,
                     itunes::parser::Library /* iTunes library */)

// Reply after parsing the Picasa PMP Database with the parser result and a
// listing of the user's Picasa albums and folders, along with metadata.
IPC_MESSAGE_CONTROL3(ChromeUtilityHostMsg_ParsePicasaPMPDatabase_Finished,
                     bool /* parse_success */,
                     std::vector<picasa::AlbumInfo> /* albums */,
                     std::vector<picasa::AlbumInfo> /* folders */)

// Reply after indexing the Picasa user-created Album contents by parsing all
// the INI files in Picasa Folders.
IPC_MESSAGE_CONTROL1(ChromeUtilityHostMsg_IndexPicasaAlbumsContents_Finished,
                     picasa::AlbumImagesMap /* albums_images */)
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

// Reply after checking the passed media file. A true result indicates that
// the file appears to be a well formed media file.
IPC_MESSAGE_CONTROL1(ChromeUtilityHostMsg_CheckMediaFile_Finished,
                     bool /* passed_checks */)

IPC_MESSAGE_CONTROL3(
    ChromeUtilityHostMsg_ParseMediaMetadata_Finished,
    bool /* parse_success */,
    base::DictionaryValue /* metadata */,
    std::vector<metadata::AttachedImage> /* attached_images */)

IPC_MESSAGE_CONTROL3(ChromeUtilityHostMsg_RequestBlobBytes,
                     int64 /* request_id */,
                     int64 /* start_byte */,
                     int64 /* length */)

// Reply when a write or verify operation succeeds.
IPC_MESSAGE_CONTROL0(ChromeUtilityHostMsg_ImageWriter_Succeeded)

// Reply when a write or verify operation has been fully cancelled.
IPC_MESSAGE_CONTROL0(ChromeUtilityHostMsg_ImageWriter_Cancelled)

// Reply when a write or verify operation fails to complete.
IPC_MESSAGE_CONTROL1(ChromeUtilityHostMsg_ImageWriter_Failed,
                     std::string /* message */)

// Periodic status update about the progress of an operation.
IPC_MESSAGE_CONTROL1(ChromeUtilityHostMsg_ImageWriter_Progress,
                     int64 /* number of bytes processed */)

#if defined(OS_WIN)
// Get plain-text WiFi credentials from the system (requires UAC privilege
// elevation).
IPC_MESSAGE_CONTROL1(ChromeUtilityHostMsg_GetWiFiCredentials,
                     std::string /* ssid */)

// Reply after getting WiFi credentials from the system. |success| is false if
// error occurred.
IPC_MESSAGE_CONTROL2(ChromeUtilityHostMsg_GotWiFiCredentials,
                     std::string /* key_data */,
                     bool /* success */)
#endif  // defined(OS_WIN)
