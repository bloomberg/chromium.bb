// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/gtk_dnd_util.h"

#include <string>

#include "base/logging.h"
#include "base/pickle.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"

static const int kBitsPerByte = 8;

namespace {

void AddTargetToList(GtkTargetList* targets, int target_code) {
  switch (target_code) {
    case gtk_dnd_util::TEXT_PLAIN:
      gtk_target_list_add_text_targets(targets, gtk_dnd_util::TEXT_PLAIN);
      break;

    case gtk_dnd_util::TEXT_URI_LIST:
      gtk_target_list_add_uri_targets(targets, gtk_dnd_util::TEXT_URI_LIST);
      break;

    case gtk_dnd_util::TEXT_HTML:
      gtk_target_list_add(
          targets, gtk_dnd_util::GetAtomForTarget(gtk_dnd_util::TEXT_HTML),
          0, gtk_dnd_util::TEXT_HTML);
      break;

    case gtk_dnd_util::NETSCAPE_URL:
      gtk_target_list_add(targets,
          gtk_dnd_util::GetAtomForTarget(gtk_dnd_util::NETSCAPE_URL),
          0, gtk_dnd_util::NETSCAPE_URL);
      break;

    case gtk_dnd_util::CHROME_TAB:
    case gtk_dnd_util::CHROME_BOOKMARK_ITEM:
    case gtk_dnd_util::CHROME_NAMED_URL:
      gtk_target_list_add(targets, gtk_dnd_util::GetAtomForTarget(target_code),
                          GTK_TARGET_SAME_APP, target_code);
      break;

    case gtk_dnd_util::DIRECT_SAVE_FILE:
      gtk_target_list_add(targets,
          gtk_dnd_util::GetAtomForTarget(gtk_dnd_util::DIRECT_SAVE_FILE),
          0, gtk_dnd_util::DIRECT_SAVE_FILE);
      break;

    default:
      NOTREACHED() << " Unexpected target code: " << target_code;
  }
}

}  // namespace

namespace gtk_dnd_util {

GdkAtom GetAtomForTarget(int target) {
  switch (target) {
    case CHROME_TAB:
      static GdkAtom tab_atom = gdk_atom_intern(
          const_cast<char*>("application/x-chrome-tab"), false);
      return tab_atom;

    case TEXT_HTML:
      static GdkAtom html_atom = gdk_atom_intern(
          const_cast<char*>("text/html"), false);
      return html_atom;

    case CHROME_BOOKMARK_ITEM:
      static GdkAtom bookmark_atom = gdk_atom_intern(
          const_cast<char*>("application/x-chrome-bookmark-item"), false);
      return bookmark_atom;

    case TEXT_PLAIN:
      static GdkAtom text_atom = gdk_atom_intern(
          const_cast<char*>("text/plain;charset=utf-8"), false);
      return text_atom;

    case TEXT_URI_LIST:
      static GdkAtom uris_atom = gdk_atom_intern(
          const_cast<char*>("text/uri-list"), false);
      return uris_atom;

    case CHROME_NAMED_URL:
      static GdkAtom named_url = gdk_atom_intern(
          const_cast<char*>("application/x-chrome-named-url"), false);
      return named_url;

    case NETSCAPE_URL:
      static GdkAtom netscape_url = gdk_atom_intern(
          const_cast<char*>("_NETSCAPE_URL"), false);
      return netscape_url;

    case TEXT_PLAIN_NO_CHARSET:
      static GdkAtom text_no_charset_atom = gdk_atom_intern(
          const_cast<char*>("text/plain"), false);
      return text_no_charset_atom;

    case DIRECT_SAVE_FILE:
      static GdkAtom xds_atom = gdk_atom_intern(
          const_cast<char*>("XdndDirectSave0"), false);
      return xds_atom;

    default:
      NOTREACHED();
  }

  return NULL;
}

GtkTargetList* GetTargetListFromCodeMask(int code_mask) {
  GtkTargetList* targets = gtk_target_list_new(NULL, 0);

  for (size_t i = 1; i < INVALID_TARGET; i = i << 1) {
    if (i == CHROME_WEBDROP_FILE_CONTENTS)
      continue;

    if (i & code_mask)
      AddTargetToList(targets, i);
  }

  return targets;
}

void SetSourceTargetListFromCodeMask(GtkWidget* source, int code_mask) {
  GtkTargetList* targets = GetTargetListFromCodeMask(code_mask);
  gtk_drag_source_set_target_list(source, targets);
  gtk_target_list_unref(targets);
}

void SetDestTargetList(GtkWidget* dest, const int* target_codes) {
  GtkTargetList* targets = gtk_target_list_new(NULL, 0);

  for (size_t i = 0; target_codes[i] != -1; ++i) {
    AddTargetToList(targets, target_codes[i]);
  }

  gtk_drag_dest_set_target_list(dest, targets);
  gtk_target_list_unref(targets);
}

void WriteURLWithName(GtkSelectionData* selection_data,
                      const GURL& url,
                      string16 title,
                      int type) {
  if (title.empty()) {
    // We prefer to not have empty titles. Set it to the filename extracted
    // from the URL.
    title = UTF8ToUTF16(url.ExtractFileName());
  }

  switch (type) {
    case TEXT_PLAIN: {
      gtk_selection_data_set_text(selection_data, url.spec().c_str(),
                                  url.spec().length());
      break;
    }
    case TEXT_URI_LIST: {
      gchar* uri_array[2];
      uri_array[0] = strdup(url.spec().c_str());
      uri_array[1] = NULL;
      gtk_selection_data_set_uris(selection_data, uri_array);
      free(uri_array[0]);
      break;
    }
    case CHROME_NAMED_URL: {
      Pickle pickle;
      pickle.WriteString(UTF16ToUTF8(title));
      pickle.WriteString(url.spec());
      gtk_selection_data_set(
          selection_data,
          GetAtomForTarget(gtk_dnd_util::CHROME_NAMED_URL),
          kBitsPerByte,
          reinterpret_cast<const guchar*>(pickle.data()),
          pickle.size());
      break;
    }
    case NETSCAPE_URL: {
      // _NETSCAPE_URL format is URL + \n + title.
      std::string utf8_text = url.spec() + "\n" + UTF16ToUTF8(title);
      gtk_selection_data_set(selection_data,
                             selection_data->target,
                             kBitsPerByte,
                             reinterpret_cast<const guchar*>(utf8_text.c_str()),
                             utf8_text.length());
      break;
    }

    default: {
      NOTREACHED();
      break;
    }
  }
}

bool ExtractNamedURL(GtkSelectionData* selection_data,
                     GURL* url,
                     string16* title) {
  if (!selection_data || selection_data->length <= 0)
    return false;

  Pickle data(reinterpret_cast<char*>(selection_data->data),
              selection_data->length);
  void* iter = NULL;
  std::string title_utf8, url_utf8;
  if (!data.ReadString(&iter, &title_utf8) ||
      !data.ReadString(&iter, &url_utf8)) {
    return false;
  }

  GURL gurl(url_utf8);
  if (!gurl.is_valid())
    return false;

  *url = gurl;
  *title = UTF8ToUTF16(title_utf8);
  return true;
}

bool ExtractURIList(GtkSelectionData* selection_data, std::vector<GURL>* urls) {
  gchar** uris = gtk_selection_data_get_uris(selection_data);
  if (!uris)
    return false;

  for (size_t i = 0; uris[i] != NULL; ++i) {
    GURL url(uris[i]);
    if (url.is_valid())
      urls->push_back(url);
  }

  g_strfreev(uris);
  return true;
}

}  // namespace gtk_dnd_util
