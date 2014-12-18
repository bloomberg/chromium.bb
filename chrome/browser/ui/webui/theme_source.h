// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_THEME_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_THEME_SOURCE_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/url_data_source.h"

class Profile;

namespace base {
class RefCountedMemory;
}

class ThemeSource : public content::URLDataSource {
 public:
  explicit ThemeSource(Profile* profile);
  ~ThemeSource() override;

  // content::URLDataSource implementation.
  std::string GetSource() const override;
  void StartDataRequest(
      const std::string& path,
      int render_process_id,
      int render_frame_id,
      const content::URLDataSource::GotDataCallback& callback) override;
  std::string GetMimeType(const std::string& path) const override;
  base::MessageLoop* MessageLoopForRequestPath(
      const std::string& path) const override;
  bool ShouldReplaceExistingSource() const override;
  bool ShouldServiceRequest(const net::URLRequest* request) const override;

 private:
  // Fetch and send the theme bitmap.
  void SendThemeBitmap(const content::URLDataSource::GotDataCallback& callback,
                       int resource_id,
                       float scale_factor);

  // Similar to SendThemeBitmap but treat the responded data as image; if the
  // resource bundle does not contain the data for |scale_factor|, the resource
  // bundle falls back to the data of a lower scale, which means smaller images
  // will be served and webui handles the image incorrectly.
  // See crbug.com/442384.
  void SendThemeImage(const content::URLDataSource::GotDataCallback& callback,
                      int resource_id,
                      float scale_factor);

  // The original profile (never an OTR profile).
  Profile* profile_;

  // We grab the CSS early so we don't have to go back to the UI thread later.
  scoped_refptr<base::RefCountedMemory> css_bytes_;

  DISALLOW_COPY_AND_ASSIGN(ThemeSource);
};

#endif  // CHROME_BROWSER_UI_WEBUI_THEME_SOURCE_H_
