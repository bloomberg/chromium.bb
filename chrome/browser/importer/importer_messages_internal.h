// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "chrome/browser/history/history_types.h"
#include "chrome/browser/importer/importer_data_types.h"
#include "ipc/ipc_message_macros.h"
#include "webkit/glue/password_form.h"

//-----------------------------------------------------------------------------
// ProfileImportProcess messages
// These are messages sent from the browser to the profile import process.
IPC_BEGIN_MESSAGES(ProfileImportProcess)
  IPC_MESSAGE_CONTROL4(ProfileImportProcessMsg_StartImport,
                       importer::ProfileInfo /* ProfileInfo struct */,
                       int  /* bitmask of items to import */,
                       DictionaryValue  /* localized strings */,
                       bool  /* import to bookmark bar */)

  IPC_MESSAGE_CONTROL0(ProfileImportProcessMsg_CancelImport)

  IPC_MESSAGE_CONTROL1(ProfileImportProcessMsg_ReportImportItemFinished,
                       int  /* ImportItem */)
IPC_END_MESSAGES(ProfileImportProcess)

//---------------------------------------------------------------------------
// ProfileImportProcessHost messages
// These are messages sent from the profile import process to the browser.
IPC_BEGIN_MESSAGES(ProfileImportProcessHost)
  // These messages send information about the status of the import and
  // individual import tasks.
  IPC_MESSAGE_CONTROL0(ProfileImportProcessHostMsg_Import_Started)

  IPC_MESSAGE_CONTROL2(ProfileImportProcessHostMsg_Import_Finished,
                       bool  /* was import successful? */,
                       std::string  /* error message, if any */)

  IPC_MESSAGE_CONTROL1(ProfileImportProcessHostMsg_ImportItem_Started,
                       int  /* ImportItem */)

  IPC_MESSAGE_CONTROL1(ProfileImportProcessHostMsg_ImportItem_Finished,
                       int  /* ImportItem */)

  // These messages send data from the external importer process back to
  // the process host so it can be written to the profile.
  IPC_MESSAGE_CONTROL1(ProfileImportProcessHostMsg_NotifyHistoryImportStart,
                       int  /* total number of history::URLRow items */)

  IPC_MESSAGE_CONTROL2(ProfileImportProcessHostMsg_NotifyHistoryImportGroup,
                       std::vector<history::URLRow>,
                       int  /* the source of URLs as in history::VisitSource.*/
                            /* To simplify IPC call, pass as an integer */)

  IPC_MESSAGE_CONTROL1(ProfileImportProcessHostMsg_NotifyHomePageImportReady,
                       GURL  /* GURL of home page */)

  IPC_MESSAGE_CONTROL3(ProfileImportProcessHostMsg_NotifyBookmarksImportStart,
                       std::wstring  /* first folder name */,
                       int  /* options */,
                       int  /* total number of bookmarks */)

  IPC_MESSAGE_CONTROL1(ProfileImportProcessHostMsg_NotifyBookmarksImportGroup,
                       std::vector<ProfileWriter::BookmarkEntry>)

  IPC_MESSAGE_CONTROL1(ProfileImportProcessHostMsg_NotifyFavIconsImportStart,
                       int  /* total number of FavIcons */)

  IPC_MESSAGE_CONTROL1(ProfileImportProcessHostMsg_NotifyFavIconsImportGroup,
                       std::vector<history::ImportedFavIconUsage> )

  IPC_MESSAGE_CONTROL1(ProfileImportProcessHostMsg_NotifyPasswordFormReady,
                       webkit_glue::PasswordForm )

  IPC_MESSAGE_CONTROL3(ProfileImportProcessHostMsg_NotifyKeywordsReady,
                       std::vector<TemplateURL>,
                       int,  /* default keyword index */
                       bool  /* unique on host and path */)
IPC_END_MESSAGES(ProfileImportProcessHost)

