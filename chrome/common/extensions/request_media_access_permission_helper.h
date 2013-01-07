// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_REQUEST_MEDIA_ACCESS_PERMISSION_HELPER_H_
#define CHROME_COMMON_EXTENSIONS_REQUEST_MEDIA_ACCESS_PERMISSION_HELPER_H_

#include "base/callback.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/common/media_stream_request.h"

namespace content{
typedef base::Callback< void(const MediaStreamDevices&) > MediaResponseCallback;
}  // namespace content

namespace extensions {

class RequestMediaAccessPermissionHelper {
 public:
  static void AuthorizeRequest(const content::MediaStreamDevices& devices,
                               const content::MediaStreamRequest& request,
                               const content::MediaResponseCallback& callback,
                               const extensions::Extension* extension,
                               bool is_packaged_app);

 private:
  RequestMediaAccessPermissionHelper();
  ~RequestMediaAccessPermissionHelper();

  DISALLOW_COPY_AND_ASSIGN(RequestMediaAccessPermissionHelper);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_REQUEST_MEDIA_ACCESS_PERMISSION_HELPER_H_
