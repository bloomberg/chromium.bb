// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_MAIN_PLACEHOLDER_CONTENT_H_
#define ATHENA_MAIN_PLACEHOLDER_CONTENT_H_

namespace content {
class BrowserContext;
}

// Creates WebContents with a pre-defined set of windows.
void CreateTestPages(content::BrowserContext* browser_context);

#endif  // ATHENA_MAIN_PLACEHOLDER_CONTENT_H_
