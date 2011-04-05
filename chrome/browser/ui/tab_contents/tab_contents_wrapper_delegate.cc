// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/tab_contents_wrapper_delegate.h"

TabContentsWrapperDelegate::~TabContentsWrapperDelegate() {
}

// Notification when an application programmatically requests installation.
void TabContentsWrapperDelegate::OnInstallApplication(
    TabContentsWrapper* source,
    const WebApplicationInfo& app_info) {
}

void TabContentsWrapperDelegate::OnDidGetApplicationInfo(
    TabContentsWrapper* source, int32 page_id) {
}
