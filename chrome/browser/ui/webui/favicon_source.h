// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FAVICON_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_FAVICON_SOURCE_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/common/cancelable_task_tracker.h"
#include "content/public/browser/url_data_source.h"
#include "ui/gfx/favicon_size.h"

class Profile;

// FaviconSource is the gateway between network-level chrome:
// requests for favicons and the history backend that serves these.
class FaviconSource : public content::URLDataSource {
 public:
  // Defines the type of icon the FaviconSource will provide.
  enum IconType {
    FAVICON,
    // Any available icon in the priority of TOUCH_ICON_PRECOMPOSED, TOUCH_ICON,
    // FAVICON, and default favicon.
    ANY
  };

  // |type| is the type of icon this FaviconSource will provide.
  FaviconSource(Profile* profile, IconType type);

  // content::URLDataSource implementation.
  virtual std::string GetSource() OVERRIDE;
  virtual void StartDataRequest(
      const std::string& path,
      bool is_incognito,
      const content::URLDataSource::GotDataCallback& callback) OVERRIDE;
  virtual std::string GetMimeType(const std::string&) const OVERRIDE;
  virtual bool ShouldReplaceExistingSource() const OVERRIDE;

 protected:
  struct IconRequest {
    IconRequest();
    IconRequest(const content::URLDataSource::GotDataCallback& cb,
                const std::string& path,
                int size,
                ui::ScaleFactor scale);
    ~IconRequest();

    content::URLDataSource::GotDataCallback callback;
    std::string request_path;
    int size_in_dip;
    ui::ScaleFactor scale_factor;
  };

  virtual ~FaviconSource();

  // Called when the favicon data is missing to perform additional checks to
  // locate the resource.
  // |request| contains information for the failed request.
  // Returns true if the missing resource is found.
  virtual bool HandleMissingResource(const IconRequest& request);

  Profile* profile_;

 private:
  // Defines the allowed pixel sizes for requested favicons.
  enum IconSize {
    SIZE_16,
    SIZE_32,
    SIZE_64,
    NUM_SIZES
  };

  // Called when favicon data is available from the history backend.
  void OnFaviconDataAvailable(
      const IconRequest& request,
      const history::FaviconBitmapResult& bitmap_result);

  // Sends the default favicon.
  void SendDefaultResponse(const IconRequest& request);

  CancelableTaskTracker cancelable_task_tracker_;

  // Raw PNG representations of favicons of each size to show when the favicon
  // database doesn't have a favicon for a webpage. Indexed by IconSize values.
  scoped_refptr<base::RefCountedMemory> default_favicons_[NUM_SIZES];

  // The history::IconTypes of icon that this FaviconSource handles.
  int icon_types_;

  DISALLOW_COPY_AND_ASSIGN(FaviconSource);
};

#endif  // CHROME_BROWSER_UI_WEBUI_FAVICON_SOURCE_H_
