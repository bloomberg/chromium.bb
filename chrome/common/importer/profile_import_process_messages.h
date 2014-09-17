// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, no traditonal include guard.
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "base/values.h"
#include "chrome/common/common_param_traits_macros.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "chrome/common/importer/imported_favicon_usage.h"
#include "chrome/common/importer/importer_autofill_form_data_entry.h"
#include "chrome/common/importer/importer_data_types.h"
#include "chrome/common/importer/importer_url_row.h"
#include "chrome/common/importer/profile_import_process_param_traits_macros.h"
#include "components/autofill/content/common/autofill_param_traits_macros.h"
#include "components/autofill/core/common/password_form.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"

// Force multiple inclusion of the param traits file to generate all methods.
#undef CHROME_COMMON_IMPORTER_PROFILE_IMPORT_PROCESS_PARAM_TRAITS_MACROS_H_

#define IPC_MESSAGE_START ProfileImportMsgStart

//-----------------------------------------------------------------------------
// ProfileImportProcess messages
// These are messages sent from the browser to the profile import process.
IPC_MESSAGE_CONTROL3(ProfileImportProcessMsg_StartImport,
                     importer::SourceProfile,
                     int                     /* Bitmask of items to import. */,
                     base::DictionaryValue   /* Localized strings. */)

IPC_MESSAGE_CONTROL0(ProfileImportProcessMsg_CancelImport)

IPC_MESSAGE_CONTROL1(ProfileImportProcessMsg_ReportImportItemFinished,
                     int  /* ImportItem */)

//---------------------------------------------------------------------------
// ProfileImportProcessHost messages
// These are messages sent from the profile import process to the browser.
// These messages send information about the status of the import and
// individual import tasks.
IPC_MESSAGE_CONTROL0(ProfileImportProcessHostMsg_Import_Started)

IPC_MESSAGE_CONTROL2(ProfileImportProcessHostMsg_Import_Finished,
                     bool         /* was import successful? */,
                     std::string  /* error message, if any */)

IPC_MESSAGE_CONTROL1(ProfileImportProcessHostMsg_ImportItem_Started,
                     int  /* ImportItem */)

IPC_MESSAGE_CONTROL1(ProfileImportProcessHostMsg_ImportItem_Finished,
                     int  /* ImportItem */)

// These messages send data from the external importer process back to
// the process host so it can be written to the profile.
IPC_MESSAGE_CONTROL1(ProfileImportProcessHostMsg_NotifyHistoryImportStart,
                     int  /* total number of ImporterURLRow items */)

IPC_MESSAGE_CONTROL2(ProfileImportProcessHostMsg_NotifyHistoryImportGroup,
                     std::vector<ImporterURLRow>,
                     int  /* the source of URLs as in history::VisitSource.*/
                          /* To simplify IPC call, pass as an integer */)

IPC_MESSAGE_CONTROL1(ProfileImportProcessHostMsg_NotifyHomePageImportReady,
                     GURL  /* GURL of home page */)

IPC_MESSAGE_CONTROL2(ProfileImportProcessHostMsg_NotifyBookmarksImportStart,
                     base::string16  /* first folder name */,
                     int             /* total number of bookmarks */)

IPC_MESSAGE_CONTROL1(ProfileImportProcessHostMsg_NotifyBookmarksImportGroup,
                     std::vector<ImportedBookmarkEntry>)

IPC_MESSAGE_CONTROL1(ProfileImportProcessHostMsg_NotifyFaviconsImportStart,
                     int  /* total number of favicons */)

IPC_MESSAGE_CONTROL1(ProfileImportProcessHostMsg_NotifyFaviconsImportGroup,
                     std::vector<ImportedFaviconUsage>)

IPC_MESSAGE_CONTROL1(ProfileImportProcessHostMsg_NotifyPasswordFormReady,
                     autofill::PasswordForm)

IPC_MESSAGE_CONTROL2(ProfileImportProcessHostMsg_NotifyKeywordsReady,
                     std::vector<importer::URLKeywordInfo>, // url_keywords
                     bool  /* unique on host and path */)

IPC_MESSAGE_CONTROL1(ProfileImportProcessHostMsg_NotifyFirefoxSearchEngData,
                     std::vector<std::string>) // search_engine_data

IPC_MESSAGE_CONTROL1(ProfileImportProcessHostMsg_AutofillFormDataImportStart,
                     int  /* total number of entries to be imported */)

IPC_MESSAGE_CONTROL1(ProfileImportProcessHostMsg_AutofillFormDataImportGroup,
                     std::vector<ImporterAutofillFormDataEntry>)

#if defined(OS_WIN)
IPC_MESSAGE_CONTROL1(ProfileImportProcessHostMsg_NotifyIE7PasswordInfo,
                     importer::ImporterIE7PasswordInfo) // password_info
#endif
