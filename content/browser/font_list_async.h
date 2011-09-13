// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FONT_LIST_ASYNC_H_
#define CONTENT_BROWSER_FONT_LIST_ASYNC_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"

namespace base {
class ListValue;
}

namespace content {

// Wraps ownership of a ListValue so we can send it across threads without
// having ownership problems (for example if the calling thread goes away
// before the callback is executed, we want the ListValue to be destroyed.
struct CONTENT_EXPORT FontListResult
    : public base::RefCountedThreadSafe<FontListResult> {
  FontListResult();
  ~FontListResult();

  scoped_ptr<base::ListValue> list;
};

// Retrieves the list of fonts on the system as a list of strings. It provides
// a non-blocking interface to GetFontList_SlowBlocking in common/.
//
// This function will run asynchronously on a background thread since getting
// the font list from the system can be slow. This function may be called from
// any thread that has a BrowserThread::ID. The callback will be executed on
// the calling thread.
//
// If the caller wants to take ownership of the ListValue, it can just do
// FontListResult.list.release() or use scoped_ptr.swap() because this value
// isn't used for anything else.
CONTENT_EXPORT void GetFontListAsync(
    const base::Callback<void(scoped_refptr<FontListResult>)>& callback);

}  // namespace content

#endif  // CONTENT_BROWSER_FONT_LIST_ASYNC_H_
