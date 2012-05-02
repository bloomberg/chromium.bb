// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_CONTENT_CLIENT_INITIALIZER_
#define CONTENT_TEST_TEST_CONTENT_CLIENT_INITIALIZER_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

class NotificationServiceImpl;

namespace content {

class ContentClient;
class ContentBrowserClient;

// Initializes various objects needed to run unit tests that use content::
// objects. Currently this includes setting up the notification service,
// creating and setting the content client and the content browser client.
class TestContentClientInitializer {
 public:
  TestContentClientInitializer();
  ~TestContentClientInitializer();

 private:
  scoped_ptr<NotificationServiceImpl> notification_service_;
  scoped_ptr<content::ContentClient> content_client_;
  scoped_ptr<content::ContentBrowserClient> content_browser_client_;

  DISALLOW_COPY_AND_ASSIGN(TestContentClientInitializer);
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_CONTENT_CLIENT_INITIALIZER_
