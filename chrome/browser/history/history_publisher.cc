// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/history_publisher.h"

#include "base/strings/utf_string_conversions.h"

namespace history {

void HistoryPublisher::PublishPageContent(const base::Time& time,
                                          const GURL& url,
                                          const string16& title,
                                          const string16& contents) const {
  PageData page_data = {
    time,
    url,
    contents.c_str(),
    title.c_str(),
  };

  PublishDataToIndexers(page_data);
}

}  // namespace history
