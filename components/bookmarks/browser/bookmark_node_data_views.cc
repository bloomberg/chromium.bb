// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_node_data.h"

#include "base/logging.h"
#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/clipboard/clipboard.h"

namespace {

const char kJavaScriptScheme[] = "javascript";

}  // namespace

// static
const ui::OSExchangeData::CustomFormat&
BookmarkNodeData::GetBookmarkCustomFormat() {
  CR_DEFINE_STATIC_LOCAL(
      ui::OSExchangeData::CustomFormat,
      format,
      (ui::Clipboard::GetFormatType(BookmarkNodeData::kClipboardFormatString)));

  return format;
}

void BookmarkNodeData::Write(const base::FilePath& profile_path,
                             ui::OSExchangeData* data) const {
  DCHECK(data);

  // If there is only one element and it is a URL, write the URL to the
  // clipboard.
  if (elements.size() == 1 && elements[0].is_url) {
    if (elements[0].url.SchemeIs(kJavaScriptScheme)) {
      data->SetString(base::UTF8ToUTF16(elements[0].url.spec()));
    } else {
      data->SetURL(elements[0].url, elements[0].title);
    }
  }

  Pickle data_pickle;
  WriteToPickle(profile_path, &data_pickle);

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
    base::string16 title;
    if (data.GetURLAndTitle(
            ui::OSExchangeData::CONVERT_FILENAMES, &url, &title))
      ReadFromTuple(url, title);
  }

  return is_valid();
}
