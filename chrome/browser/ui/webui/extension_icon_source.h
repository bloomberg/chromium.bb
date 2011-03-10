// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EXTENSION_ICON_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_EXTENSION_ICON_SOURCE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/browser/favicon_service.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/common/extensions/extension.h"
#include "third_party/skia/include/core/SkBitmap.h"

class ExtensionIconSet;
class Profile;
class RefCountedMemory;

namespace gfx {
class Size;
}

// ExtensionIconSource serves extension icons through network level chrome:
// requests. Icons can be retrieved for any installed extension or app.
//
// The format for requesting an icon is as follows:
//   chrome://extension-icon/<extension_id>/<icon_size>/<match_type>?[options]
//
//   Parameters (<> required, [] optional):
//    <extension_id>  = the id of the extension
//    <icon_size>     = the size of the icon, as the integer value of the
//                      corresponding Extension:Icons enum.
//    <match_type>    = the fallback matching policy, as the integer value of
//                      the corresponding ExtensionIconSet::MatchType enum.
//    [options]       = Optional transformations to apply. Supported options:
//                        grayscale=true to desaturate the image.
//
// Examples:
//   chrome-extension://gbmgkahjioeacddebbnengilkgbkhodg/32/1?grayscale=true
//     (ICON_SMALL, MATCH_BIGGER, grayscale)
//   chrome-extension://gbmgkahjioeacddebbnengilkgbkhodg/128/0
//     (ICON_LARGE, MATCH_EXACTLY)
//
// We attempt to load icons from the following sources in order:
//  1) The icons as listed in the extension / app manifests.
//  2) If a 16px icon was requested, the favicon for extension's launch URL.
//  3) The default extension / application icon if there are still no matches.
//
class ExtensionIconSource : public ChromeURLDataManager::DataSource,
                            public ImageLoadingTracker::Observer {
 public:
  explicit ExtensionIconSource(Profile* profile);
  virtual ~ExtensionIconSource();

  // Gets the URL of the |extension| icon in the given |size|, falling back
  // based on the |match| type. If |grayscale|, the URL will be for the
  // desaturated version of the icon.
  static GURL GetIconURL(const Extension* extension,
                         Extension::Icons icon_size,
                         ExtensionIconSet::MatchType match,
                         bool grayscale);

  // ChromeURLDataManager::DataSource

  virtual std::string GetMimeType(const std::string&) const;

  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id);


 private:
  // Encapsulates the request parameters for |request_id|.
  struct ExtensionIconRequest;

  // Returns the bitmap for the default app image.
  SkBitmap* GetDefaultAppImage();

  // Returns the bitmap for the default extension.
  SkBitmap* GetDefaultExtensionImage();

  // Performs any remaining transformations (like desaturating the |image|),
  // then returns the |image| to the client and clears up any temporary data
  // associated with the |request_id|.
  void FinalizeImage(SkBitmap* image, int request_id);

  // Loads the default image for |request_id| and returns to the client.
  void LoadDefaultImage(int request_id);

  // Loads the extension's |icon| for the given |request_id| and returns the
  // image to the client.
  void LoadExtensionImage(ExtensionResource icon, int request_id);

  // Loads the favicon image for the app associated with the |request_id|. If
  // the image does not exist, we fall back to the default image.
  void LoadFaviconImage(int request_id);

  // FaviconService callback
  void OnFaviconDataAvailable(FaviconService::Handle request_handle,
                              bool know_favicon,
                              scoped_refptr<RefCountedMemory> data,
                              bool expired,
                              GURL icon_url);

  // ImageLoadingTracker::Observer
  virtual void OnImageLoaded(SkBitmap* image,
                             ExtensionResource resource,
                             int id);

  // Called when the extension doesn't have an icon. We fall back to multiple
  // sources, using the following order:
  //  1) The icons as listed in the extension / app manifests.
  //  2) If a 16px icon and the extension has a launch URL, see if Chrome
  //     has a corresponding favicon.
  //  3) If still no matches, load the default extension / application icon.
  void LoadIconFailed(int request_id);

  // Parses and savse an ExtensionIconRequest for the URL |path| for the
  // specified |request_id|.
  bool ParseData(const std::string& path, int request_id);

  // Sends the default response to |request_id|, used for invalid requests.
  void SendDefaultResponse(int request_id);

  // Stores the parameters associated with the |request_id|, making them
  // as an ExtensionIconRequest via GetData.
  void SetData(int request_id,
               const Extension* extension,
               bool grayscale,
               Extension::Icons size,
               ExtensionIconSet::MatchType match);

  // Returns the ExtensionIconRequest for the given |request_id|.
  ExtensionIconRequest* GetData(int request_id);

  // Removes temporary data associated with |request_id|.
  void ClearData(int request_id);

  Profile* profile_;

  // Maps tracker ids to request ids.
  std::map<int, int> tracker_map_;

  // Maps request_ids to ExtensionIconRequests.
  std::map<int, ExtensionIconRequest*> request_map_;

  scoped_ptr<ImageLoadingTracker> tracker_;

  int next_tracker_id_;

  scoped_ptr<SkBitmap> default_app_data_;

  scoped_ptr<SkBitmap> default_extension_data_;

  CancelableRequestConsumerT<int, 0> cancelable_consumer_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionIconSource);
};

#endif  // CHROME_BROWSER_UI_WEBUI_EXTENSION_ICON_SOURCE_H_
