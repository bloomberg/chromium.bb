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
#include "chrome/common/importer/importer_data_types.h"
#include "chrome/common/importer/importer_url_row.h"
#include "components/autofill/content/common/autofill_param_traits_macros.h"
#include "components/autofill/core/common/password_form.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"

#ifndef CHROME_COMMON_IMPORTER_PROFILE_IMPORT_PROCESS_MESSAGES_H_
#define CHROME_COMMON_IMPORTER_PROFILE_IMPORT_PROCESS_MESSAGES_H_

namespace IPC {

// Traits for importer::SourceProfile struct to pack/unpack.
template <>
struct ParamTraits<importer::SourceProfile> {
  typedef importer::SourceProfile param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.importer_name);
    WriteParam(m, static_cast<int>(p.importer_type));
    WriteParam(m, p.source_path);
    WriteParam(m, p.app_path);
    WriteParam(m, static_cast<int>(p.services_supported));
    WriteParam(m, p.locale);
  }
  static bool Read(const Message* m, PickleIterator* iter, param_type* p) {
    if (!ReadParam(m, iter, &p->importer_name))
      return false;

    int importer_type = 0;
    if (!ReadParam(m, iter, &importer_type))
      return false;
    p->importer_type = static_cast<importer::ImporterType>(importer_type);

    if (!ReadParam(m, iter, &p->source_path) ||
        !ReadParam(m, iter, &p->app_path)) {
        return false;
    }

    int services_supported = 0;
    if (!ReadParam(m, iter, &services_supported))
      return false;
    p->services_supported = static_cast<uint16>(services_supported);

    if (!ReadParam(m, iter, &p->locale))
      return false;

    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("(");
    LogParam(p.importer_name, l);
    l->append(", ");
    LogParam(static_cast<int>(p.importer_type), l);
    l->append(", ");
    LogParam(p.source_path, l);
    l->append(", ");
    LogParam(p.app_path, l);
    l->append(", ");
    LogParam(static_cast<int>(p.services_supported), l);
    l->append(", ");
    LogParam(p.locale, l);
    l->append(")");
  }
};  // ParamTraits<importer::SourceProfile>

// Traits for ImporterURLRow to pack/unpack.
template <>
struct ParamTraits<ImporterURLRow> {
  typedef ImporterURLRow param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.url);
    WriteParam(m, p.title);
    WriteParam(m, p.visit_count);
    WriteParam(m, p.typed_count);
    WriteParam(m, p.last_visit);
    WriteParam(m, p.hidden);
  }
  static bool Read(const Message* m, PickleIterator* iter, param_type* p) {
    GURL url;
    base::string16 title;
    int visit_count, typed_count;
    base::Time last_visit;
    bool hidden;
    if (!ReadParam(m, iter, &url) ||
        !ReadParam(m, iter, &title) ||
        !ReadParam(m, iter, &visit_count) ||
        !ReadParam(m, iter, &typed_count) ||
        !ReadParam(m, iter, &last_visit) ||
        !ReadParam(m, iter, &hidden))
      return false;
    *p = ImporterURLRow(url);
    p->title = title;
    p->visit_count = visit_count;
    p->typed_count = typed_count;
    p->last_visit = last_visit;
    p->hidden = hidden;
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("(");
    LogParam(p.url, l);
    l->append(", ");
    LogParam(p.title, l);
    l->append(", ");
    LogParam(p.visit_count, l);
    l->append(", ");
    LogParam(p.typed_count, l);
    l->append(", ");
    LogParam(p.last_visit, l);
    l->append(", ");
    LogParam(p.hidden, l);
    l->append(")");
  }
};  // ParamTraits<ImporterURLRow>

// Traits for ImportedBookmarkEntry to pack/unpack.
template <>
struct ParamTraits<ImportedBookmarkEntry> {
  typedef ImportedBookmarkEntry param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.in_toolbar);
    WriteParam(m, p.is_folder);
    WriteParam(m, p.url);
    WriteParam(m, p.path);
    WriteParam(m, p.title);
    WriteParam(m, p.creation_time);
  }
  static bool Read(const Message* m, PickleIterator* iter, param_type* p) {
    return
        (ReadParam(m, iter, &p->in_toolbar)) &&
        (ReadParam(m, iter, &p->is_folder)) &&
        (ReadParam(m, iter, &p->url)) &&
        (ReadParam(m, iter, &p->path)) &&
        (ReadParam(m, iter, &p->title)) &&
        (ReadParam(m, iter, &p->creation_time));
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("(");
    LogParam(p.in_toolbar, l);
    l->append(", ");
    LogParam(p.is_folder, l);
    l->append(", ");
    LogParam(p.url, l);
    l->append(", ");
    LogParam(p.path, l);
    l->append(", ");
    LogParam(p.title, l);
    l->append(", ");
    LogParam(p.creation_time, l);
    l->append(")");
  }
};  // ParamTraits<ImportedBookmarkEntry>

// Traits for ImportedFaviconUsage.
template <>
struct ParamTraits<ImportedFaviconUsage> {
  typedef ImportedFaviconUsage param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.favicon_url);
    WriteParam(m, p.png_data);
    WriteParam(m, p.urls);
  }
  static bool Read(const Message* m, PickleIterator* iter, param_type* p) {
    return
        ReadParam(m, iter, &p->favicon_url) &&
        ReadParam(m, iter, &p->png_data) &&
        ReadParam(m, iter, &p->urls);
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("(");
    LogParam(p.favicon_url, l);
    l->append(", ");
    LogParam(p.png_data, l);
    l->append(", ");
    LogParam(p.urls, l);
    l->append(")");
  }
};  // ParamTraits<ImportedFaviconUsage>

// Traits for importer::URLKeywordInfo
template <>
struct ParamTraits<importer::URLKeywordInfo> {
  typedef importer::URLKeywordInfo param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.url);
    WriteParam(m, p.keyword);
    WriteParam(m, p.display_name);
  }

  static bool Read(const Message* m, PickleIterator* iter, param_type* p) {
    return
        ReadParam(m, iter, &p->url) &&
        ReadParam(m, iter, &p->keyword) &&
        ReadParam(m, iter, &p->display_name);
  }

  static void Log(const param_type& p, std::string* l) {
    l->append("(");
    LogParam(p.url, l);
    l->append(", ");
    LogParam(p.keyword, l);
    l->append(", ");
    LogParam(p.display_name, l);
    l->append(")");
  }
};  // ParamTraits<importer::URLKeywordInfo>

#if defined(OS_WIN)
// Traits for importer::ImporterIE7PasswordInfo
template <>
struct ParamTraits<importer::ImporterIE7PasswordInfo> {
  typedef importer::ImporterIE7PasswordInfo param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.url_hash);
    WriteParam(m, p.encrypted_data);
    WriteParam(m, p.date_created);
  }

  static bool Read(const Message* m, PickleIterator* iter, param_type* p) {
    return
        ReadParam(m, iter, &p->url_hash) &&
        ReadParam(m, iter, &p->encrypted_data) &&
        ReadParam(m, iter, &p->date_created);
  }

  static void Log(const param_type& p, std::string* l) {
    l->append("(");
    LogParam(p.url_hash, l);
    l->append(", ");
    LogParam(p.encrypted_data, l);
    l->append(", ");
    LogParam(p.date_created, l);
    l->append(")");
  }
};  // ParamTraits<importer::ImporterIE7PasswordInfo>
#endif

}  // namespace IPC

#endif  // CHROME_COMMON_IMPORTER_PROFILE_IMPORT_PROCESS_MESSAGES_H_

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

#if defined(OS_WIN)
IPC_MESSAGE_CONTROL1(ProfileImportProcessHostMsg_NotifyIE7PasswordInfo,
                     importer::ImporterIE7PasswordInfo) // password_info
#endif
