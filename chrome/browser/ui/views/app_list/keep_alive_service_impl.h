// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_LIST_KEEP_ALIVE_SERVICE_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_APP_LIST_KEEP_ALIVE_SERVICE_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/views/app_list/keep_alive_service.h"

class ScopedKeepAlive {
 public:
  ScopedKeepAlive();
  ~ScopedKeepAlive();

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedKeepAlive);
};

class KeepAliveServiceImpl : public KeepAliveService {
 public:
  KeepAliveServiceImpl();
  virtual ~KeepAliveServiceImpl();

  // KeepAliveServiceImpl overrides.
  virtual void EnsureKeepAlive() OVERRIDE;
  virtual void FreeKeepAlive() OVERRIDE;

 private:
  // Used to keep the browser process alive while the app list is visible.
  scoped_ptr<ScopedKeepAlive> keep_alive_;

  DISALLOW_COPY_AND_ASSIGN(KeepAliveServiceImpl);
};



#endif  // CHROME_BROWSER_UI_VIEWS_APP_LIST_KEEP_ALIVE_SERVICE_IMPL_H_
