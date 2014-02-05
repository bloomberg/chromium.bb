// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_shutdown.h"

#include "base/memory/scoped_vector.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"

void DestroyBrowserWebContents(Browser* browser) {
  ScopedVector<content::WebContents> contents;
  while (browser->tab_strip_model()->count())
    contents.push_back(browser->tab_strip_model()->DetachWebContentsAt(0));
}
