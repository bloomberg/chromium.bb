// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_node_data.h"

#include <string>

#include "base/basictypes.h"
#include "base/pickle.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/common/url_constants.h"
#include "net/base/escape.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

#if defined(OS_MACOSX)
#include "chrome/browser/bookmarks/bookmark_pasteboard_helper_mac.h"
#endif

const char* BookmarkNodeData::kClipboardFormatString =
    "chromium/x-bookmark-entries";

BookmarkNodeData::Element::Element() : is_url(false), id_(0) {
}

BookmarkNodeData::Element::Element(const BookmarkNode* node)
    : is_url(node->is_url()),
      url(node->url()),
      title(node->GetTitle()),
      id_(node->id()) {
  for (int i = 0; i < node->child_count(); ++i)
    children.push_back(Element(node->GetChild(i)));
}

BookmarkNodeData::Element::~Element() {
}

void BookmarkNodeData::Element::WriteToPickle(Pickle* pickle) const {
  pickle->WriteBool(is_url);
  pickle->WriteString(url.spec());
  pickle->WriteString16(title);
  pickle->WriteInt64(id_);
  if (!is_url) {
    pickle->WriteUInt64(children.size());
    for (std::vector<Element>::const_iterator i = children.begin();
         i != children.end(); ++i) {
      i->WriteToPickle(pickle);
    }
  }
}

bool BookmarkNodeData::Element::ReadFromPickle(Pickle* pickle,
                                               PickleIterator* iterator) {
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
    uint64 children_count;
    if (!pickle->ReadUInt64(iterator, &children_count))
      return false;
    children.reserve(children_count);
    for (uint64 i = 0; i < children_count; ++i) {
      children.push_back(Element());
      if (!children.back().ReadFromPickle(pickle, iterator))
        return false;
    }
  }
  return true;
}

#if defined(TOOLKIT_VIEWS)
// static
ui::OSExchangeData::CustomFormat BookmarkNodeData::GetBookmarkCustomFormat() {
  CR_DEFINE_STATIC_LOCAL(ui::OSExchangeData::CustomFormat, format, ());
  static bool format_valid = false;

  if (!format_valid) {
    format_valid = true;
    format = ui::OSExchangeData::RegisterCustomFormat(
        BookmarkNodeData::kClipboardFormatString);
  }
  return format;
}
#endif

BookmarkNodeData::BookmarkNodeData() {
}

BookmarkNodeData::BookmarkNodeData(const BookmarkNode* node) {
  elements.push_back(Element(node));
}

BookmarkNodeData::BookmarkNodeData(
    const std::vector<const BookmarkNode*>& nodes) {
  ReadFromVector(nodes);
}

BookmarkNodeData::~BookmarkNodeData() {
}

bool BookmarkNodeData::ReadFromVector(
    const std::vector<const BookmarkNode*>& nodes) {
  Clear();

  if (nodes.empty())
    return false;

  for (size_t i = 0; i < nodes.size(); ++i)
    elements.push_back(Element(nodes[i]));

  return true;
}

bool BookmarkNodeData::ReadFromTuple(const GURL& url, const string16& title) {
  Clear();

  if (!url.is_valid())
    return false;

  Element element;
  element.title = title;
  element.url = url;
  element.is_url = true;

  elements.push_back(element);

  return true;
}

#if !defined(OS_MACOSX)
void BookmarkNodeData::WriteToClipboard(Profile* profile) const {
  ui::ScopedClipboardWriter scw(ui::Clipboard::GetForCurrentThread(),
                                ui::Clipboard::BUFFER_STANDARD,
                                content::BrowserContext::
                                    GetMarkerForOffTheRecordContext(profile));

  // If there is only one element and it is a URL, write the URL to the
  // clipboard.
  if (elements.size() == 1 && elements[0].is_url) {
    const string16& title = elements[0].title;
    const std::string url = elements[0].url.spec();

    scw.WriteBookmark(title, url);
    scw.WriteHyperlink(net::EscapeForHTML(title), url);

    // Also write the URL to the clipboard as text so that it can be pasted
    // into text fields. We use WriteText instead of WriteURL because we don't
    // want to clobber the X clipboard when the user copies out of the omnibox
    // on Linux (on Windows and Mac, there is no difference between these
    // functions).
    scw.WriteText(UTF8ToUTF16(url));
  }

  Pickle pickle;
  WriteToPickle(profile, &pickle);
  scw.WritePickledData(
      pickle, ui::Clipboard::GetFormatType(kClipboardFormatString));
}

bool BookmarkNodeData::ReadFromClipboard() {
  std::string data;
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  clipboard->ReadData(ui::Clipboard::GetFormatType(kClipboardFormatString),
                      &data);

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

bool BookmarkNodeData::ClipboardContainsBookmarks() {
  return ui::Clipboard::GetForCurrentThread()->IsFormatAvailable(
      ui::Clipboard::GetFormatType(kClipboardFormatString),
      ui::Clipboard::BUFFER_STANDARD);
}
#else
void BookmarkNodeData::WriteToClipboard(Profile* profile) const {
  bookmark_pasteboard_helper_mac::WriteToPasteboard(
      bookmark_pasteboard_helper_mac::kCopyPastePasteboard,
      elements,
      profile_path_,
      content::BrowserContext::GetMarkerForOffTheRecordContext(profile));
}

bool BookmarkNodeData::ReadFromClipboard() {
  base::FilePath file_path;
  if (!bookmark_pasteboard_helper_mac::ReadFromPasteboard(
      bookmark_pasteboard_helper_mac::kCopyPastePasteboard,
      elements,
      &file_path)) {
    return false;
  }

  profile_path_ = file_path;
  return true;
}

bool BookmarkNodeData::ReadFromDragClipboard() {
  base::FilePath file_path;
  if (!bookmark_pasteboard_helper_mac::ReadFromPasteboard(
      bookmark_pasteboard_helper_mac::kDragPasteboard,
      elements,
      &file_path)) {
    return false;
  }

  profile_path_ = file_path;
  return true;
}

bool BookmarkNodeData::ClipboardContainsBookmarks() {
  return bookmark_pasteboard_helper_mac::PasteboardContainsBookmarks(
      bookmark_pasteboard_helper_mac::kCopyPastePasteboard);
}
#endif  // !defined(OS_MACOSX)

#if defined(TOOLKIT_VIEWS)
void BookmarkNodeData::Write(Profile* profile, ui::OSExchangeData* data) const {
  DCHECK(data);

  // If there is only one element and it is a URL, write the URL to the
  // clipboard.
  if (elements.size() == 1 && elements[0].is_url) {
    if (elements[0].url.SchemeIs(chrome::kJavaScriptScheme)) {
      data->SetString(UTF8ToUTF16(elements[0].url.spec()));
    } else {
      data->SetURL(elements[0].url, elements[0].title);
    }
  }

  Pickle data_pickle;
  WriteToPickle(profile, &data_pickle);

  data->SetPickledData(GetBookmarkCustomFormat(), data_pickle);
}

bool BookmarkNodeData::Read(const ui::OSExchangeData& data) {
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
    GURL url;
    string16 title;
    if (data.GetURLAndTitle(&url, &title))
      ReadFromTuple(url, title);
  }

  return is_valid();
}
#endif

void BookmarkNodeData::WriteToPickle(Profile* profile, Pickle* pickle) const {
  base::FilePath path = profile ? profile->GetPath() : base::FilePath();
  path.WriteToPickle(pickle);
  pickle->WriteUInt64(elements.size());

  for (size_t i = 0; i < elements.size(); ++i)
    elements[i].WriteToPickle(pickle);
}

bool BookmarkNodeData::ReadFromPickle(Pickle* pickle) {
  PickleIterator data_iterator(*pickle);
  uint64 element_count;
  if (profile_path_.ReadFromPickle(&data_iterator) &&
      pickle->ReadUInt64(&data_iterator, &element_count)) {
    std::vector<Element> tmp_elements;
    tmp_elements.resize(element_count);
    for (uint64 i = 0; i < element_count; ++i) {
      if (!tmp_elements[i].ReadFromPickle(pickle, &data_iterator)) {
        return false;
      }
    }
    elements.swap(tmp_elements);
  }

  return true;
}

std::vector<const BookmarkNode*> BookmarkNodeData::GetNodes(
    Profile* profile) const {
  std::vector<const BookmarkNode*> nodes;

  if (!IsFromProfile(profile))
    return nodes;

  for (size_t i = 0; i < elements.size(); ++i) {
    const BookmarkNode* node = BookmarkModelFactory::GetForProfile(
        profile)->GetNodeByID(elements[i].id_);
    if (!node) {
      nodes.clear();
      return nodes;
    }
    nodes.push_back(node);
  }
  return nodes;
}

const BookmarkNode* BookmarkNodeData::GetFirstNode(Profile* profile) const {
  std::vector<const BookmarkNode*> nodes = GetNodes(profile);
  return nodes.size() == 1 ? nodes[0] : NULL;
}

void BookmarkNodeData::Clear() {
  profile_path_.clear();
  elements.clear();
}

void BookmarkNodeData::SetOriginatingProfile(Profile* profile) {
  DCHECK(profile_path_.empty());

  if (profile)
    profile_path_ = profile->GetPath();
}

bool BookmarkNodeData::IsFromProfile(Profile* profile) const {
  // An empty path means the data is not associated with any profile.
  return !profile_path_.empty() && profile_path_ == profile->GetPath();
}
