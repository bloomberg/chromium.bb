// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FAVICON_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_FAVICON_SOURCE_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "content/public/browser/url_data_source.h"
#include "ui/gfx/favicon_size.h"

class Profile;

// FaviconSource is the gateway between network-level chrome:
// requests for favicons and the history backend that serves these.
//
// Format:
//   chrome://favicon/size&scalefactor/urlmodifier/url
// Some parameters are optional as described below. However, the order of the
// parameters is not interchangeable.
//
// Parameter:
//  'url'               Required
//    Specifies the page URL of the requested favicon. If the 'urlmodifier'
//    parameter is 'iconurl', the URL refers to the URL of the favicon image
//    instead.
//  'size&scalefactor'  Optional
//    Values: ['largest', size/aa@bx/]
//    'largest': Specifies that the largest available favicon is requested.
//      Example: chrome://favicon/largest/http://www.google.com/
//    'size/aa@bx/':
//      Specifies the requested favicon's size in DIP (aa) and the requested
//      favicon's scale factor. (b).
//      The supported requested DIP sizes are: 16x16, 32x32 and 64x64.
//      If the parameter is unspecified, the requested favicon's size defaults
//      to 16 and the requested scale factor defaults to 1x.
//      Example: chrome://favicon/size/16@2x/http://www.google.com/
//  'urlmodifier'      Optional
//    Values: ['iconurl', 'origin']
//    'iconurl': Specifies that the url parameter refers to the URL of
//    the favicon image as opposed to the URL of the page that the favicon is
//    on.
//    Example: chrome://favicon/iconurl/http://www.google.com/favicon.ico
//    'origin': Specifies that the URL should be converted to a form with
//    an empty path and a valid scheme. The converted URL will be used to
//    request the favicon from the favicon service.
//    Examples:
//      chrome://favicon/origin/http://example.com/a
//      chrome://favicon/origin/example.com
//        Both URLs request the favicon for http://example.com from the
//        favicon service.
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

  virtual ~FaviconSource();

  // content::URLDataSource implementation.
  virtual std::string GetSource() const OVERRIDE;
  virtual void StartDataRequest(
      const std::string& path,
      int render_process_id,
      int render_frame_id,
      const content::URLDataSource::GotDataCallback& callback) OVERRIDE;
  virtual std::string GetMimeType(const std::string&) const OVERRIDE;
  virtual bool ShouldReplaceExistingSource() const OVERRIDE;
  virtual bool ShouldServiceRequest(
      const net::URLRequest* request) const OVERRIDE;

 protected:
  struct IconRequest {
    IconRequest();
    IconRequest(const content::URLDataSource::GotDataCallback& cb,
                const GURL& path,
                int size,
                float scale);
    ~IconRequest();

    content::URLDataSource::GotDataCallback callback;
    GURL request_path;
    int size_in_dip;
    float device_scale_factor;
  };

  // Called when the favicon data is missing to perform additional checks to
  // locate the resource.
  // |request| contains information for the failed request.
  // Returns true if the missing resource is found.
  virtual bool HandleMissingResource(const IconRequest& request);

  Profile* profile_;

 private:
  FRIEND_TEST_ALL_PREFIXES(FaviconSourceTest, InstantParsing);
  FRIEND_TEST_ALL_PREFIXES(FaviconSourceTest, Parsing);

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
      const favicon_base::FaviconRawBitmapResult& bitmap_result);

  // Sends the 16x16 DIP 1x default favicon.
  void SendDefaultResponse(
      const content::URLDataSource::GotDataCallback& callback);

  // Sends the default favicon.
  void SendDefaultResponse(const IconRequest& request);

  base::CancelableTaskTracker cancelable_task_tracker_;

  // Raw PNG representations of favicons of each size to show when the favicon
  // database doesn't have a favicon for a webpage. Indexed by IconSize values.
  scoped_refptr<base::RefCountedMemory> default_favicons_[NUM_SIZES];

  // The favicon_base::IconTypes of icon that this FaviconSource handles.
  int icon_types_;

  DISALLOW_COPY_AND_ASSIGN(FaviconSource);
};

#endif  // CHROME_BROWSER_UI_WEBUI_FAVICON_SOURCE_H_
