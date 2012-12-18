// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_WEBKIT_TEST_HELPERS_H_
#define CONTENT_SHELL_WEBKIT_TEST_HELPERS_H_

namespace WebTestRunner {
struct WebPreferences;
}

namespace webkit_glue {
struct WebPreferences;
}

namespace content {

void ExportPreferences(const WebTestRunner::WebPreferences& from,
                       webkit_glue::WebPreferences* to);

}  // namespace content

#endif  // CONTENT_SHELL_WEBKIT_TEST_HELPERS_H_
