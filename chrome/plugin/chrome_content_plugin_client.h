// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PLUGIN_CHROME_CONTENT_PLUGIN_CLIENT_H_
#define CHROME_PLUGIN_CHROME_CONTENT_PLUGIN_CLIENT_H_

#include "content/public/plugin/content_plugin_client.h"

class ChromeContentPluginClient : public content::ContentPluginClient {
 public:
  void PreSandboxInitialization() override;
};

#endif  // CHROME_PLUGIN_CHROME_CONTENT_PLUGIN_CLIENT_H_
