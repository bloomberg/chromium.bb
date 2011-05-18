// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FONT_LIST_H_
#define CONTENT_COMMON_FONT_LIST_H_

class ListValue;

namespace content {

// Retrieves the fonts available on the current platform and returns them.
// The caller will own the returned pointer. Each entry will be a list of
// two strings, the first being the font family, and the second being the
// localized name.
//
// This function is potentially slow (the system may do a bunch of I/O) so be
// sure not to call this on a time-critical thread like the UI or I/O threads.
//
// Most callers will want to use the GetFontListAsync function in
// content/browser/font_list_async.h which does an asynchronous call.
ListValue* GetFontList_SlowBlocking();

}  // namespace content

#endif  // CONTENT_COMMON_FONT_LIST_H_
