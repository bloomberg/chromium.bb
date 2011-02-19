// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_WEB_UI_THUMBNAIL_SOURCE_H_
#define CHROME_BROWSER_DOM_UI_WEB_UI_THUMBNAIL_SOURCE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/ref_counted_memory.h"
#include "chrome/browser/webui/chrome_url_data_manager.h"

class Profile;

namespace history {
class TopSites;
}

// ThumbnailSource is the gateway between network-level chrome: requests for
// thumbnails and the history/top-sites backend that serves these.
class WebUIThumbnailSource : public ChromeURLDataManager::DataSource {
 public:
  explicit WebUIThumbnailSource(Profile* profile);

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id);

  virtual std::string GetMimeType(const std::string& path) const;

  virtual MessageLoop* MessageLoopForRequestPath(const std::string& path) const;

 private:
  virtual ~WebUIThumbnailSource();

  // Send the default thumbnail when we are missing a real one.
  void SendDefaultThumbnail(int request_id);

  // Raw PNG representation of the thumbnail to show when the thumbnail
  // database doesn't have a thumbnail for a webpage.
  scoped_refptr<RefCountedMemory> default_thumbnail_;

  // TopSites.
  scoped_refptr<history::TopSites> top_sites_;

  DISALLOW_COPY_AND_ASSIGN(WebUIThumbnailSource);
};

#endif  // CHROME_BROWSER_DOM_UI_WEB_UI_THUMBNAIL_SOURCE_H_
