// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_content_client_initializer.h"

#include "content/browser/mock_content_browser_client.h"
#include "content/browser/notification_service_impl.h"
#include "content/public/common/content_client.h"
#include "content/test/test_content_client.h"

namespace content {

TestContentClientInitializer::TestContentClientInitializer() {
  notification_service_.reset(new NotificationServiceImpl());

  DCHECK(!content::GetContentClient());
  content_client_.reset(new TestContentClient);
  content::SetContentClient(content_client_.get());

  content_browser_client_.reset(new content::MockContentBrowserClient());
  content_client_->set_browser(content_browser_client_.get());
}

TestContentClientInitializer::~TestContentClientInitializer() {
  notification_service_.reset();

  DCHECK_EQ(content_client_.get(), content::GetContentClient());
  content::SetContentClient(NULL);
  content_client_.reset();

  content_browser_client_.reset();
}

}  // namespace content
