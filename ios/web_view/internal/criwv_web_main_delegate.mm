// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/criwv_web_main_delegate.h"

#include "base/memory/ptr_util.h"
#import "ios/web_view/internal/criwv_web_client.h"
#import "ios/web_view/public/criwv_delegate.h"

namespace ios_web_view {

CRIWVWebMainDelegate::CRIWVWebMainDelegate(id<CRIWVDelegate> delegate)
    : delegate_(delegate) {}

CRIWVWebMainDelegate::~CRIWVWebMainDelegate() {}

void CRIWVWebMainDelegate::BasicStartupComplete() {
  web_client_ = base::MakeUnique<CRIWVWebClient>(delegate_);
  web::SetWebClient(web_client_.get());
}

}  // namespace ios_web_view
