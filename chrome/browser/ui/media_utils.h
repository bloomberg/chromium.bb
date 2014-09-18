// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_MEDIA_UTILS_H_
#define CHROME_BROWSER_UI_MEDIA_UTILS_H_

#include "base/callback.h"
#include "content/public/common/media_stream_request.h"

class GURL;
class Profile;

namespace content {
class WebContents;
}

void RequestMediaAccessPermission(
    content::WebContents* web_contents,
    Profile* profile,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback);

bool CheckMediaAccessPermission(content::WebContents* web_contents,
                                const GURL& security_origin,
                                content::MediaStreamType type);

#endif  // CHROME_BROWSER_UI_MEDIA_UTILS_H_
