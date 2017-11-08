// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_FILE_URL_LOADER_H_
#define CONTENT_PUBLIC_BROWSER_FILE_URL_LOADER_H_

#include "content/common/content_export.h"
#include "content/public/common/url_loader.mojom.h"

namespace content {

struct ResourceRequest;

// Helper to create a self-owned URLLoader instance which fulfills |request|
// using the contents of the file at |path|. The URL in |request| must be a
// file:// URL.
//
// Note that this does not restrict filesystem access *in any way*, so if the
// file at |path| is accessible to the browser, it will be loaded and used to
// the request.
//
// The URLLoader created by this function does *not* automatically follow
// filesytem links (e.g. Windows shortcuts) or support directory listing.
// A directory path will always yield a FILE_NOT_FOUND network error.
CONTENT_EXPORT void CreateFileURLLoader(const ResourceRequest& request,
                                        mojom::URLLoaderRequest loader,
                                        mojom::URLLoaderClientPtr client);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_FILE_URL_LOADER_H_
