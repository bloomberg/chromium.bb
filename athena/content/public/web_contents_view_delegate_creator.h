// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_CONTENT_PUBLIC_WEB_CONTENTS_VIEW_DELEGATE_FACTORY_H_
#define ATHENA_CONTENT_PUBLIC_WEB_CONTENTS_VIEW_DELEGATE_FACTORY_H_

#include "athena/athena_export.h"

namespace content {
class WebContents;
class WebContentsViewDelegate;
}

namespace athena {

ATHENA_EXPORT content::WebContentsViewDelegate* CreateWebContentsViewDelegate(
    content::WebContents* web_contents);

}  // namespace athena

#endif  // ATHENA_CONTENT_PUBLIC_WEB_CONTENTS_VIEW_DELEGATE_FACTORY_H_
