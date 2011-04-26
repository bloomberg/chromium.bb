// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_RESOURCE_GPU_BLACKLIST_UPDATER_H_
#define CHROME_BROWSER_WEB_RESOURCE_GPU_BLACKLIST_UPDATER_H_
#pragma once

#include "chrome/browser/web_resource/web_resource_service.h"

class DictionaryValue;
class GpuBlacklist;

class GpuBlacklistUpdater
    : public WebResourceService {
 public:
  explicit GpuBlacklistUpdater();

  // URL of the up-to-date gpu_blacklist.json file.
  static const char* kDefaultGpuBlacklistURL;

 private:
  virtual ~GpuBlacklistUpdater();

  virtual void Unpack(const DictionaryValue& parsed_json);

  void LoadGpuBlacklist();

  // This is the version cached in local state that's automatically updated
  // from the web.
  const DictionaryValue* gpu_blacklist_cache_;

  DISALLOW_COPY_AND_ASSIGN(GpuBlacklistUpdater);
};

#endif  // CHROME_BROWSER_WEB_RESOURCE_GPU_BLACKLIST_UPDATER_H_

