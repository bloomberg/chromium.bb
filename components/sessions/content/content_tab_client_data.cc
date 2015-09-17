// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/content/content_tab_client_data.h"

#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"

namespace sessions {

ContentTabClientData::ContentTabClientData(content::WebContents* web_contents)
    :  // TODO(ajwong): This does not correctly handle storage for isolated
       // apps.
      session_storage_namespace_(web_contents->GetController()
                                     .GetDefaultSessionStorageNamespace()) {}

ContentTabClientData::ContentTabClientData() {}

ContentTabClientData::~ContentTabClientData() {}

scoped_ptr<TabClientData> ContentTabClientData::Clone() {
  ContentTabClientData* clone = new ContentTabClientData();
  clone->session_storage_namespace_ = session_storage_namespace_;
  return make_scoped_ptr(clone);
}

}  // namespace sessions
