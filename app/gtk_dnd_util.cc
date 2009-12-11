// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/gtk_dnd_util.h"

#include "base/logging.h"
#include "base/pickle.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"

static const int kBitsPerByte = 8;

// static
GdkAtom GtkDndUtil::GetAtomForTarget(int target) {
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

    default:
      NOTREACHED();
  }

  return NULL;
}

// static
GtkTargetList* GtkDndUtil::GetTargetListFromCodeMask(int code_mask) {
  GtkTargetList* targets = gtk_target_list_new(NULL, 0);

  for (size_t i = 1; i < INVALID_TARGET; i = i << 1) {
    if (i == CHROME_WEBDROP_FILE_CONTENTS)
      continue;

    if (i & code_mask)
      AddTargetToList(targets, i);
  }

  return targets;
}

// static
void GtkDndUtil::SetSourceTargetListFromCodeMask(GtkWidget* source,
                                                 int code_mask) {
  GtkTargetList* targets = GetTargetListFromCodeMask(code_mask);
  gtk_drag_source_set_target_list(source, targets);
  gtk_target_list_unref(targets);
}

// static
void GtkDndUtil::SetDestTargetList(GtkWidget* dest, const int* target_codes) {
  GtkTargetList* targets = gtk_target_list_new(NULL, 0);

  for (size_t i = 0; target_codes[i] != -1; ++i) {
    AddTargetToList(targets, target_codes[i]);
  }

  gtk_drag_dest_set_target_list(dest, targets);
  gtk_target_list_unref(targets);
}

// static
void GtkDndUtil::AddTargetToList(GtkTargetList* targets, int target_code) {
  switch (target_code) {
    case TEXT_PLAIN:
      gtk_target_list_add_text_targets(targets, TEXT_PLAIN);
      break;

    case TEXT_URI_LIST:
      gtk_target_list_add_uri_targets(targets, TEXT_URI_LIST);
      break;

    case TEXT_HTML:
      gtk_target_list_add(targets, GetAtomForTarget(TEXT_PLAIN), 0, TEXT_HTML);
      break;

    case CHROME_TAB:
    case CHROME_BOOKMARK_ITEM:
    case CHROME_NAMED_URL:
      gtk_target_list_add(targets, GtkDndUtil::GetAtomForTarget(target_code),
                          GTK_TARGET_SAME_APP, target_code);
      break;

    default:
      NOTREACHED() << " Unexpected target code: " << target_code;
  }
}

// static
void GtkDndUtil::WriteURLWithName(GtkSelectionData* selection_data,
                                  const GURL& url,
                                  const string16& title,
                                  int type) {
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
          GetAtomForTarget(GtkDndUtil::CHROME_NAMED_URL),
          kBitsPerByte,
          reinterpret_cast<const guchar*>(pickle.data()),
          pickle.size());
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

// static
bool GtkDndUtil::ExtractNamedURL(GtkSelectionData* selection_data,
                                 GURL* url,
                                 string16* title) {
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

// static
bool GtkDndUtil::ExtractURIList(GtkSelectionData* selection_data,
                                std::vector<GURL>* urls) {
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
