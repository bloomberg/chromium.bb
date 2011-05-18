// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/font_list_async.h"

#include "base/bind.h"
#include "base/values.h"
#include "content/browser/browser_thread.h"
#include "content/common/font_list.h"

namespace content {

namespace {

// Just executes the given callback with the parameter.
void ReturnFontListToOriginalThread(
    const base::Callback<void(scoped_refptr<FontListResult>)>& callback,
    scoped_refptr<FontListResult> result) {
  callback.Run(result);
}

void GetFontListOnFileThread(
    BrowserThread::ID calling_thread_id,
    const base::Callback<void(scoped_refptr<FontListResult>)>& callback) {
  scoped_refptr<FontListResult> result(new FontListResult);
  result->list.reset(GetFontList_SlowBlocking());
  BrowserThread::PostTask(calling_thread_id, FROM_HERE,
      base::Bind(&ReturnFontListToOriginalThread, callback, result));
}

}  // namespace

FontListResult::FontListResult() {
}

FontListResult::~FontListResult() {
}

void GetFontListAsync(
    const base::Callback<void(scoped_refptr<FontListResult>)>& callback) {
  BrowserThread::ID id;
  bool well_known_thread = BrowserThread::GetCurrentThreadIdentifier(&id);
  DCHECK(well_known_thread)
      << "Can only call GetFontList from a well-known thread.";

  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
      base::Bind(&GetFontListOnFileThread, id, callback));
}

}  // namespace content
