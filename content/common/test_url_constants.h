// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_TEST_URL_CONSTANTS_H_
#define CONTENT_COMMON_TEST_URL_CONSTANTS_H_

// Contains constants for known URLs and portions thereof.
namespace content {

// Various special URLs used for testing. They have the same values as the URLs
// in chrome/common/url_constants.h, but are duplicated here so that the reverse
// dependency can be broken.

// Various URLs used in security policy testing.
extern const char kTestCacheURL[];

// The NTP is assumed in several tests to have the property that it is WebUI.
extern const char kTestNewTabURL[];

// The History page is assumed in several tests to have the property that it is
// WebUI. That's the case only with the ChromeBrowserContentClient
// implementation of WebUIFactory. With a different implementation that might
// not be the case.
extern const char kTestHistoryURL[];

// The Bookmarks page is assumed in several tests to have the property that it
// is an extension. That's the case only with the ChromeBrowserContentClient
// implementation of WebUIFactory. With a different implementation that might
// not be the case.
extern const char kTestBookmarksURL[];

}  // namespace content

#endif  // CONTENT_COMMON_TEST_URL_CONSTANTS_H_
