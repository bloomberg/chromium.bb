// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_HTML_WRITER_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_HTML_WRITER_H_

class BookmarkModel;
class FilePath;
class MessageLoop;

namespace bookmark_html_writer {

// Writes the bookmarks out in the 'bookmarks.html' format understood by
// Firefox and IE. The results are written to the file at |path|.  The file
// thread is used.
//
// TODO(sky): need a callback on failure.
void WriteBookmarks(BookmarkModel* model, const FilePath& path);

}

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_HTML_WRITER_H_
