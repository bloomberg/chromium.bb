// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_IMPORTER_MESSAGES_H_
#define CHROME_BROWSER_IMPORTER_IMPORTER_MESSAGES_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/importer/importer_data_types.h"
#include "chrome/browser/importer/profile_writer.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/common/common_param_traits.h"
#include "content/common/common_param_traits.h"
#include "ipc/ipc_message_utils.h"

namespace IPC {

// Traits for importer::ProfileInfo struct to pack/unpack.
template <>
struct ParamTraits<importer::ProfileInfo> {
  typedef importer::ProfileInfo param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.description);
    WriteParam(m, static_cast<int>(p.browser_type));
    WriteParam(m, p.source_path);
    WriteParam(m, p.app_path);
    WriteParam(m, static_cast<int>(p.services_supported));
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    if (!ReadParam(m, iter, &p->description))
      return false;

    int browser_type = 0;
    if (!ReadParam(m, iter, &browser_type))
      return false;
    p->browser_type = static_cast<importer::ProfileType>(browser_type);

    if (!ReadParam(m, iter, &p->source_path) ||
        !ReadParam(m, iter, &p->app_path))
        return false;

    int services_supported = 0;
    if (!ReadParam(m, iter, &services_supported))
      return false;
    p->services_supported = static_cast<uint16>(services_supported);

    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("(");
    LogParam(p.description, l);
    l->append(", ");
    LogParam(static_cast<int>(p.browser_type), l);
    l->append(", ");
    LogParam(p.source_path, l);
    l->append(", ");
    LogParam(p.app_path, l);
    l->append(", ");
    LogParam(static_cast<int>(p.services_supported), l);
    l->append(")");
  }
};  // ParamTraits<importer::ProfileInfo>

// Traits for history::URLRow to pack/unpack.
template <>
struct ParamTraits<history::URLRow> {
  typedef history::URLRow param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.id());
    WriteParam(m, p.url());
    WriteParam(m, p.title());
    WriteParam(m, p.visit_count());
    WriteParam(m, p.typed_count());
    WriteParam(m, p.last_visit());
    WriteParam(m, p.hidden());
    WriteParam(m, p.favicon_id());
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    history::URLID id;
    GURL url;
    string16 title;
    int visit_count, typed_count;
    base::Time last_visit;
    bool hidden;
    history::FavIconID favicon_id;
    if (!ReadParam(m, iter, &id) ||
        !ReadParam(m, iter, &url) ||
        !ReadParam(m, iter, &title) ||
        !ReadParam(m, iter, &visit_count) ||
        !ReadParam(m, iter, &typed_count) ||
        !ReadParam(m, iter, &last_visit) ||
        !ReadParam(m, iter, &hidden) ||
        !ReadParam(m, iter, &favicon_id))
      return false;
    *p = history::URLRow(url, id);
    p->set_title(title);
    p->set_visit_count(visit_count);
    p->set_typed_count(typed_count);
    p->set_last_visit(last_visit);
    p->set_hidden(hidden);
    p->set_favicon_id(favicon_id);
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("(");
    LogParam(p.id(), l);
    l->append(", ");
    LogParam(p.url(), l);
    l->append(", ");
    LogParam(p.title(), l);
    l->append(", ");
    LogParam(p.visit_count(), l);
    l->append(", ");
    LogParam(p.typed_count(), l);
    l->append(", ");
    LogParam(p.last_visit(), l);
    l->append(", ");
    LogParam(p.hidden(), l);
    l->append(", ");
    LogParam(p.favicon_id(), l);
    l->append(")");
  }
};  // ParamTraits<history::URLRow>

// Traits for ProfileWriter::BookmarkEntry to pack/unpack.
template <>
struct ParamTraits<ProfileWriter::BookmarkEntry> {
  typedef ProfileWriter::BookmarkEntry param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.in_toolbar);
    WriteParam(m, p.is_folder);
    WriteParam(m, p.url);
    WriteParam(m, p.path);
    WriteParam(m, p.title);
    WriteParam(m, p.creation_time);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
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
};  // ParamTraits<ProfileWriter::BookmarkEntry>

// Traits for history::ImportedFavIconUsage.
template <>
struct ParamTraits<history::ImportedFavIconUsage> {
  typedef history::ImportedFavIconUsage param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.favicon_url);
    WriteParam(m, p.png_data);
    WriteParam(m, p.urls);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
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
};  // ParamTraits<history::ImportedFavIconUsage

// Traits for TemplateURLRef
template <>
struct ParamTraits<TemplateURLRef> {
  typedef TemplateURLRef param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.url());
    WriteParam(m, p.index_offset());
    WriteParam(m, p.page_offset());
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    std::string url;
    int index_offset;
    int page_offset;
    if (!ReadParam(m, iter, &url) ||
        !ReadParam(m, iter, &index_offset) ||
        !ReadParam(m, iter, &page_offset))
      return false;
    *p = TemplateURLRef(url, index_offset, page_offset);
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("<TemplateURLRef>");
  }
};

// Traits for TemplateURL::ImageRef
template <>
struct ParamTraits<TemplateURL::ImageRef> {
  typedef TemplateURL::ImageRef param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.type);
    WriteParam(m, p.width);
    WriteParam(m, p.height);
    WriteParam(m, p.url);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    std::string type;
    int width;
    int height;
    GURL url;
    if (!ReadParam(m, iter, &type) ||
        !ReadParam(m, iter, &width) ||
        !ReadParam(m, iter, &height) ||
        !ReadParam(m, iter, &url))
      return false;
    *p = TemplateURL::ImageRef(type, width, height, url);  // here in
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("<TemplateURL::ImageRef>");
  }
};

// Traits for TemplateURL
template <>
struct ParamTraits<TemplateURL> {
  typedef TemplateURL param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.short_name());
    WriteParam(m, p.description());
    if (p.suggestions_url()) {
      WriteParam(m, true);
      WriteParam(m, *p.suggestions_url());
    } else {
      WriteParam(m, false);
    }
    WriteParam(m, *p.url());
    WriteParam(m, p.originating_url());
    WriteParam(m, p.keyword());
    WriteParam(m, p.autogenerate_keyword());
    WriteParam(m, p.show_in_default_list());
    WriteParam(m, p.safe_for_autoreplace());
    WriteParam(m, p.image_refs().size());

    std::vector<TemplateURL::ImageRef>::const_iterator iter;
    for (iter = p.image_refs().begin(); iter != p.image_refs().end(); ++iter) {
      WriteParam(m, iter->type);
      WriteParam(m, iter->width);
      WriteParam(m, iter->height);
      WriteParam(m, iter->url);
    }

    WriteParam(m, p.languages());
    WriteParam(m, p.input_encodings());
    WriteParam(m, p.date_created());
    WriteParam(m, p.usage_count());
    WriteParam(m, p.prepopulate_id());
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    string16 short_name;
    string16 description;
    bool includes_suggestions_url;
    TemplateURLRef suggestions_url;
    TemplateURLRef url;
    GURL originating_url;
    string16 keyword;
    bool autogenerate_keyword;
    bool show_in_default_list;
    bool safe_for_autoreplace;
    std::vector<string16> languages;
    std::vector<std::string> input_encodings;
    base::Time date_created;
    int usage_count;
    int prepopulate_id;

    if (!ReadParam(m, iter, &short_name) ||
        !ReadParam(m, iter, &description))
      return false;

    if (!ReadParam(m, iter, &includes_suggestions_url))
      return false;
    if (includes_suggestions_url) {
        if (!ReadParam(m, iter, &suggestions_url))
          return false;
    }

    size_t image_refs_size = 0;
    if (!ReadParam(m, iter, &url) ||
        !ReadParam(m, iter, &originating_url) ||
        !ReadParam(m, iter, &keyword) ||
        !ReadParam(m, iter, &autogenerate_keyword) ||
        !ReadParam(m, iter, &show_in_default_list) ||
        !ReadParam(m, iter, &safe_for_autoreplace) ||
        !ReadParam(m, iter, &image_refs_size))
      return false;

    *p = TemplateURL();
    for (size_t i = 0; i < image_refs_size; ++i) {
      std::string type;
      int width;
      int height;
      GURL url;
      if (!ReadParam(m, iter, &type) ||
          !ReadParam(m, iter, &width) ||
          !ReadParam(m, iter, &height) ||
          !ReadParam(m, iter, &url))
        return false;
      p->add_image_ref(TemplateURL::ImageRef(type, width, height, url));
    }

    if (!ReadParam(m, iter, &languages) ||
        !ReadParam(m, iter, &input_encodings) ||
        !ReadParam(m, iter, &date_created) ||
        !ReadParam(m, iter, &usage_count) ||
        !ReadParam(m, iter, &prepopulate_id))
      return false;

    p->set_short_name(short_name);
    p->set_description(description);
    p->SetSuggestionsURL(suggestions_url.url(), suggestions_url.index_offset(),
                         suggestions_url.page_offset());
    p->SetURL(url.url(), url.index_offset(), url.page_offset());
    p->set_originating_url(originating_url);
    p->set_keyword(keyword);
    p->set_autogenerate_keyword(autogenerate_keyword);
    p->set_show_in_default_list(show_in_default_list);
    p->set_safe_for_autoreplace(safe_for_autoreplace);

    std::vector<string16>::const_iterator lang_iter;
    for (lang_iter = languages.begin();
         lang_iter != languages.end();
         ++lang_iter) {
      p->add_language(*lang_iter);
    }
    p->set_input_encodings(input_encodings);
    p->set_date_created(date_created);
    p->set_usage_count(usage_count);
    p->set_prepopulate_id(prepopulate_id);
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("<TemplateURL>");
  }
};

}  // namespace IPC

#include "chrome/browser/importer/importer_messages_internal.h"

#endif  // CHROME_BROWSER_IMPORTER_IMPORTER_MESSAGES_H_
