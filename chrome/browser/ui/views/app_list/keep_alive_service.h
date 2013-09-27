// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_LIST_KEEP_ALIVE_SERVICE_H_
#define CHROME_BROWSER_UI_VIEWS_APP_LIST_KEEP_ALIVE_SERVICE_H_

// An interface to manipulate browser keepalive. Subclasses should ensure that
// keepalive is cleared when the object is destructed.
class KeepAliveService {
 public:
  virtual ~KeepAliveService() {}
  virtual void EnsureKeepAlive() = 0;
  virtual void FreeKeepAlive() = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_LIST_KEEP_ALIVE_SERVICE_H_
