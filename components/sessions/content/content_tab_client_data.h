// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CONTENT_CONTENT_CLIENT_DATA_H_
#define COMPONENTS_SESSIONS_CONTENT_CONTENT_CLIENT_DATA_H_

#include "base/memory/ref_counted.h"
#include "components/sessions/core/tab_restore_service_client.h"
#include "components/sessions/sessions_export.h"
#include "content/public/browser/session_storage_namespace.h"

namespace sessions {

// A //content-specific subclass of TabClientData that is used to associate
// TabRestoreService::Tab instances with the content::SessionStorageNamespace
// of the WebContents from which they were created.
class SESSIONS_EXPORT ContentTabClientData : public TabClientData {
 public:
  explicit ContentTabClientData(content::WebContents* web_contents);
  ContentTabClientData();
  ~ContentTabClientData() override;

  content::SessionStorageNamespace* session_storage_namespace() const {
    return session_storage_namespace_.get();
  }

 private:
  // TabClientData:
  scoped_ptr<TabClientData> Clone() override;

  scoped_refptr<content::SessionStorageNamespace> session_storage_namespace_;
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CONTENT_CONTENT_CLIENT_DATA_H_
