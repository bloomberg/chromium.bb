// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, so no include guard.

#include <stdint.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "chrome/common/media_galleries/itunes_library.h"
#include "chrome/common/media_galleries/metadata_types.h"
#include "chrome/common/media_galleries/picasa_types.h"
#include "extensions/features/features.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"

#if !BUILDFLAG(ENABLE_EXTENSIONS)
#error "Extensions must be enabled"
#endif

#define IPC_MESSAGE_START ChromeUtilityExtensionsMsgStart

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

//------------------------------------------------------------------------------
// Utility process messages:
// These are messages from the browser to the utility process.


#if defined(OS_WIN)
// Tell the utility process to parse the iTunes preference XML file contents
// and return the path to the iTunes directory.
IPC_MESSAGE_CONTROL1(ChromeUtilityMsg_ParseITunesPrefXml,
                     std::string /* XML to parse */)
#endif  // defined(OS_WIN)

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

//------------------------------------------------------------------------------
// Utility process host messages:
// These are messages from the utility process to the browser.

#if defined(OS_WIN)
// Reply after parsing the iTunes preferences XML file contents with either the
// path to the iTunes directory or an empty FilePath.
IPC_MESSAGE_CONTROL1(ChromeUtilityHostMsg_GotITunesDirectory,
                     base::FilePath /* Path to iTunes library */)
#endif  // defined(OS_WIN)

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
