// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FAVICON_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_FAVICON_SOURCE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "chrome/browser/favicon_service.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"

class GURL;
class Profile;

// FaviconSource is the gateway between network-level chrome:
// requests for favicons and the history backend that serves these.
class FaviconSource : public ChromeURLDataManager::DataSource {
 public:
  explicit FaviconSource(Profile* profile);

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id);

  virtual std::string GetMimeType(const std::string&) const;

  virtual bool ShouldReplaceExistingSource() const;

 private:
  // Called when favicon data is available from the history backend.
  void OnFaviconDataAvailable(FaviconService::Handle request_handle,
                              bool know_favicon,
                              scoped_refptr<RefCountedMemory> data,
                              bool expired,
                              GURL url);

  // Sends the default favicon.
  void SendDefaultResponse(int request_id);

  virtual ~FaviconSource();

  Profile* profile_;
  CancelableRequestConsumerT<int, 0> cancelable_consumer_;

  // Raw PNG representation of the favicon to show when the favicon
  // database doesn't have a favicon for a webpage.
  scoped_refptr<RefCountedMemory> default_favicon_;

  DISALLOW_COPY_AND_ASSIGN(FaviconSource);
};

#endif  // CHROME_BROWSER_UI_WEBUI_FAVICON_SOURCE_H_
