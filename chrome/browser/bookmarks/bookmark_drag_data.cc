// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_drag_data.h"

#include "app/clipboard/scoped_clipboard_writer.h"
#include "base/basictypes.h"
#include "base/pickle.h"
#include "base/string_util.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/browser/browser_process.h"
#include "net/base/escape.h"

const char* BookmarkDragData::kClipboardFormatString =
    "chromium/x-bookmark-entries";

BookmarkDragData::Element::Element(const BookmarkNode* node)
    : is_url(node->is_url()),
      url(node->GetURL()),
      title(node->GetTitleAsString16()),
      id_(node->id()) {
  for (int i = 0; i < node->GetChildCount(); ++i)
    children.push_back(Element(node->GetChild(i)));
}

void BookmarkDragData::Element::WriteToPickle(Pickle* pickle) const {
  pickle->WriteBool(is_url);
  pickle->WriteString(url.spec());
  pickle->WriteString16(title);
  pickle->WriteInt64(id_);
  if (!is_url) {
    pickle->WriteSize(children.size());
    for (std::vector<Element>::const_iterator i = children.begin();
         i != children.end(); ++i) {
      i->WriteToPickle(pickle);
    }
  }
}

bool BookmarkDragData::Element::ReadFromPickle(Pickle* pickle,
                                               void** iterator) {
  std::string url_spec;
  if (!pickle->ReadBool(iterator, &is_url) ||
      !pickle->ReadString(iterator, &url_spec) ||
      !pickle->ReadString16(iterator, &title) ||
      !pickle->ReadInt64(iterator, &id_)) {
    return false;
  }
  url = GURL(url_spec);
  children.clear();
  if (!is_url) {
    size_t children_count;
    if (!pickle->ReadSize(iterator, &children_count))
      return false;
    children.resize(children_count);
    for (std::vector<Element>::iterator i = children.begin();
         i != children.end(); ++i) {
      if (!i->ReadFromPickle(pickle, iterator))
        return false;
    }
  }
  return true;
}

#if defined(TOOLKIT_VIEWS)
// static
OSExchangeData::CustomFormat BookmarkDragData::GetBookmarkCustomFormat() {
  static OSExchangeData::CustomFormat format;
  static bool format_valid = false;

  if (!format_valid) {
    format_valid = true;
    format = OSExchangeData::RegisterCustomFormat(
        BookmarkDragData::kClipboardFormatString);
  }
  return format;
}
#endif

BookmarkDragData::BookmarkDragData(const BookmarkNode* node) {
  elements.push_back(Element(node));
}

BookmarkDragData::BookmarkDragData(
    const std::vector<const BookmarkNode*>& nodes) {
  for (size_t i = 0; i < nodes.size(); ++i)
    elements.push_back(Element(nodes[i]));
}

#if !defined(OS_MACOSX)
void BookmarkDragData::WriteToClipboard(Profile* profile) const {
  ScopedClipboardWriter scw(g_browser_process->clipboard());

  // If there is only one element and it is a URL, write the URL to the
  // clipboard.
  if (elements.size() == 1 && elements[0].is_url) {
    const string16& title = elements[0].title;
    std::string url = elements[0].url.spec();

    scw.WriteBookmark(title, url);
    scw.WriteHyperlink(EscapeForHTML(UTF16ToUTF8(title)), url);

    // Also write the URL to the clipboard as text so that it can be pasted
    // into text fields
    scw.WriteURL(UTF8ToUTF16(url));
  }

  Pickle pickle;
  WriteToPickle(profile, &pickle);
  scw.WritePickledData(pickle, kClipboardFormatString);
}

bool BookmarkDragData::ReadFromClipboard() {
  std::string data;
  Clipboard* clipboard = g_browser_process->clipboard();
  clipboard->ReadData(kClipboardFormatString, &data);

  if (!data.empty()) {
    Pickle pickle(data.data(), data.size());
    if (ReadFromPickle(&pickle))
      return true;
  }

  string16 title;
  std::string url;
  clipboard->ReadBookmark(&title, &url);
  if (!url.empty()) {
    Element element;
    element.is_url = true;
    element.url = GURL(url);
    element.title = title;

    elements.clear();
    elements.push_back(element);
    return true;
  }

  return false;
}
#endif  // !defined(OS_MACOSX)

#if defined(TOOLKIT_VIEWS)
void BookmarkDragData::Write(Profile* profile, OSExchangeData* data) const {
  DCHECK(data);

  // If there is only one element and it is a URL, write the URL to the
  // clipboard.
  if (elements.size() == 1 && elements[0].is_url) {
    if (elements[0].url.SchemeIs(chrome::kJavaScriptScheme)) {
      data->SetString(ASCIIToWide(elements[0].url.spec()));
    } else {
      data->SetURL(elements[0].url, UTF16ToWide(elements[0].title));
    }
  }

  Pickle data_pickle;
  WriteToPickle(profile, &data_pickle);

  data->SetPickledData(GetBookmarkCustomFormat(), data_pickle);
}

bool BookmarkDragData::Read(const OSExchangeData& data) {
  elements.clear();

  profile_path_.clear();

  if (data.HasCustomFormat(GetBookmarkCustomFormat())) {
    Pickle drag_data_pickle;
    if (data.GetPickledData(GetBookmarkCustomFormat(), &drag_data_pickle)) {
      if (!ReadFromPickle(&drag_data_pickle))
        return false;
    }
  } else {
    // See if there is a URL on the clipboard.
    Element element;
    std::wstring title;
    if (data.GetURLAndTitle(&element.url, &title) &&
        element.url.is_valid()) {
      element.title = WideToUTF16(title);
      element.is_url = true;
      elements.push_back(element);
    }
  }

  return is_valid();
}
#endif

void BookmarkDragData::WriteToPickle(Profile* profile, Pickle* pickle) const {
  FilePath path = profile ? profile->GetPath() : FilePath();
  FilePath::WriteStringTypeToPickle(pickle, path.value());
  pickle->WriteSize(elements.size());

  for (size_t i = 0; i < elements.size(); ++i)
    elements[i].WriteToPickle(pickle);
}

bool BookmarkDragData::ReadFromPickle(Pickle* pickle) {
  void* data_iterator = NULL;
  size_t element_count;
  if (FilePath::ReadStringTypeFromPickle(pickle, &data_iterator,
                                         &profile_path_) &&
      pickle->ReadSize(&data_iterator, &element_count)) {
    std::vector<Element> tmp_elements;
    tmp_elements.resize(element_count);
    for (size_t i = 0; i < element_count; ++i) {
      if (!tmp_elements[i].ReadFromPickle(pickle, &data_iterator)) {
        return false;
      }
    }
    elements.swap(tmp_elements);
  }

  return true;
}

std::vector<const BookmarkNode*> BookmarkDragData::GetNodes(
    Profile* profile) const {
  std::vector<const BookmarkNode*> nodes;

  if (!IsFromProfile(profile))
    return nodes;

  for (size_t i = 0; i < elements.size(); ++i) {
    const BookmarkNode* node =
        profile->GetBookmarkModel()->GetNodeByID(elements[i].id_);
    if (!node) {
      nodes.clear();
      return nodes;
    }
    nodes.push_back(node);
  }
  return nodes;
}

const BookmarkNode* BookmarkDragData::GetFirstNode(Profile* profile) const {
  std::vector<const BookmarkNode*> nodes = GetNodes(profile);
  return nodes.size() == 1 ? nodes[0] : NULL;
}

bool BookmarkDragData::IsFromProfile(Profile* profile) const {
  // An empty path means the data is not associated with any profile.
  return (!profile_path_.empty() &&
#if defined(WCHAR_T_IS_UTF16)
      profile->GetPath().ToWStringHack() == profile_path_);
#elif defined(WCHAR_T_IS_UTF32)
      profile->GetPath().value() == profile_path_);
#endif
}
