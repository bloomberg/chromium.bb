// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_SYNC_INTERNALS_HTML_SOURCE_H_
#define CHROME_BROWSER_DOM_UI_SYNC_INTERNALS_HTML_SOURCE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/webui/chrome_url_data_manager.h"

class SyncInternalsHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  SyncInternalsHTMLSource();

  // ChromeURLDataManager::DataSource implementation.
  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id);
  virtual std::string GetMimeType(const std::string& path) const;

 protected:
  virtual ~SyncInternalsHTMLSource();

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncInternalsHTMLSource);
};

#endif  // CHROME_BROWSER_DOM_UI_SYNC_INTERNALS_HTML_SOURCE_H_
