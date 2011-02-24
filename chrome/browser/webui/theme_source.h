// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBUI_THEME_SOURCE_H_
#define CHROME_BROWSER_WEBUI_THEME_SOURCE_H_
#pragma once

#include <string>

#include "chrome/browser/webui/chrome_url_data_manager.h"

class Profile;
class RefCountedBytes;

class ThemeSource : public ChromeURLDataManager::DataSource {
 public:
  explicit ThemeSource(Profile* profile);

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id);
  virtual std::string GetMimeType(const std::string& path) const;

  // Used to tell ChromeURLDataManager which thread to handle the request on.
  virtual MessageLoop* MessageLoopForRequestPath(const std::string& path) const;

  virtual bool ShouldReplaceExistingSource() const;

 protected:
  virtual ~ThemeSource();

 private:
  // Fetch and send the theme bitmap.
  void SendThemeBitmap(int request_id, int resource_id);

  // The original profile (never an OTR profile).
  Profile* profile_;

  // We grab the CSS early so we don't have to go back to the UI thread later.
  scoped_refptr<RefCountedBytes> css_bytes_;

  DISALLOW_COPY_AND_ASSIGN(ThemeSource);
};

#endif  // CHROME_BROWSER_WEBUI_THEME_SOURCE_H_
