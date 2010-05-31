// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USER_IMAGE_DOWNLOADER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USER_IMAGE_DOWNLOADER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/utility_process_host.h"
#include "chrome/common/net/url_fetcher.h"

class ListValue;
class ResourceDispatcherHost;

namespace chromeos {

// Gets user image URL from user's Google Profile, downloads the image,
// converts it to PNG format and stores the converted image in a file with
// path to it stored in local state dictionary.
class UserImageDownloader : public URLFetcher::Delegate,
                            public UtilityProcessHost::Client {
 public:
  // Starts downloading the picture upon successful login.
  // |auth_token| is a authentication token received in ClientLogin
  // response, used for requests sent to Contacts API.
  UserImageDownloader(const std::string& username,
                      const std::string& auth_token);

 private:
  // It's a reference counted object, so destructor is private.
  ~UserImageDownloader();

  // Overriden from URLFetcher::Delegate:
  virtual void OnURLFetchComplete(const URLFetcher* source,
                                  const GURL& url,
                                  const URLRequestStatus& status,
                                  int response_code,
                                  const ResponseCookies& cookies,
                                  const std::string& data);

  // Overriden from UtilityProcessHost::Client:
  virtual void OnDecodeImageSucceeded(const SkBitmap& decoded_image);

  // Launches sandboxed process that will decode the image.
  void DecodeImageInSandbox(ResourceDispatcherHost* rdh,
                            const std::vector<unsigned char>& image_data);

  // Parses received JSON data looking for user image url.
  // If succeeded, returns true and stores the url in |image_url| parameter.
  // Otherwise, returns false.
  bool GetImageURL(const std::string& json_data, GURL* image_url) const;

  // Searches for image url in a list of contacts matching contact address
  // with user email. Returns true and image url if succeeds, false
  // otherwise.
  bool GetImageURLFromEntries(ListValue* entry_list, GURL* image_url) const;

  // Checks if email list contains user email. Returns true if match is
  // found.
  bool IsUserEntry(ListValue* email_list) const;

  // Searches for image url in list of links for the found contact.
  // Returns true and image url if succeeds, false otherwise.
  bool GetImageURLFromLinks(ListValue* link_list, GURL* image_url) const;

  // Encodes user image in PNG format and saves the result to the file
  // specified. Should work on IO thread.
  void SaveImageAsPNG(const std::string& filename, const SkBitmap& image);

  // Fetcher for user's profile page.
  scoped_ptr<URLFetcher> profile_fetcher_;

  // Fetcher for user's profile picture.
  scoped_ptr<URLFetcher> picture_fetcher_;

  // Username saved to use as a key for user picture in preferences.
  std::string username_;

  // Authentication token to use for image download.
  std::string auth_token_;

  DISALLOW_COPY_AND_ASSIGN(UserImageDownloader);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USER_IMAGE_DOWNLOADER_H_

