// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_LARGE_ICON_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_LARGE_ICON_SOURCE_H_

#include "base/memory/scoped_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon_base/fallback_icon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "content/public/browser/url_data_source.h"

class Profile;

// LargeIconSource services explicit chrome:// requests for large icons.
//
// Format:
//   chrome://large-icon/size/url
//
// Parameter:
//  'size'             Required (including trailing '/')
//    Positive integer to specify the large icon's size in pixels.
//  'url'              Optional
//    String to specify the page URL of the large icon.
//
//  Example: chrome://large-icon/48/http://www.google.com/
//    This requests a 48x48 large icon for http://www.google.com.
class LargeIconSource : public content::URLDataSource {
 public:
  explicit LargeIconSource(Profile* profile);

  ~LargeIconSource() override;

  // content::URLDataSource implementation.
  std::string GetSource() const override;
  void StartDataRequest(
      const std::string& path,
      int render_process_id,
      int render_frame_id,
      const content::URLDataSource::GotDataCallback& callback) override;
  std::string GetMimeType(const std::string&) const override;
  bool ShouldReplaceExistingSource() const override;
  bool ShouldServiceRequest(const net::URLRequest* request) const override;

 protected:
  struct IconRequest {
    IconRequest();
    IconRequest(const content::URLDataSource::GotDataCallback& callback_in,
                const GURL& path_in,
                int size_in);
    ~IconRequest();

    content::URLDataSource::GotDataCallback callback;
    GURL url;
    int size;
  };

 private:
  // Callback for icon data retrieval request.
  void OnIconDataAvailable(
      const IconRequest& request,
      const favicon_base::FaviconRawBitmapResult& bitmap_result);

  // Renders and sends a fallback icon.
  void SendFallbackIcon(const IconRequest& request);

  // Returns null to trigger "Not Found" response.
  void SendNotFoundResponse(
      const content::URLDataSource::GotDataCallback& callback);

  Profile* profile_;

  base::CancelableTaskTracker cancelable_task_tracker_;

  scoped_ptr<favicon_base::FallbackIconService> fallback_icon_service_;

  DISALLOW_COPY_AND_ASSIGN(LargeIconSource);
};

#endif  // CHROME_BROWSER_UI_WEBUI_LARGE_ICON_SOURCE_H_
