// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBREQUEST_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBREQUEST_API_H_
#pragma once

#include <string>

#include "base/singleton.h"

class ExtensionIOEventRouter;
class GURL;

// IO thread
class ExtensionWebRequestEventRouter {
 public:
  static ExtensionWebRequestEventRouter* GetInstance();

  // TODO(mpcomplete): additional params
  void OnBeforeRequest(const ExtensionIOEventRouter* event_router,
                       const GURL& url, const std::string& method);

 private:
  friend struct DefaultSingletonTraits<ExtensionWebRequestEventRouter>;

  ExtensionWebRequestEventRouter();
  ~ExtensionWebRequestEventRouter();

  DISALLOW_COPY_AND_ASSIGN(ExtensionWebRequestEventRouter);
};


#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBREQUEST_API_H_
