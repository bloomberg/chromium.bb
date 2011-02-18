// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/dom_ui/chrome_url_data_manager.h"
#include "chrome/common/url_constants.h"

class RemotingResourcesSource : public ChromeURLDataManager::DataSource {
 public:
  RemotingResourcesSource();
  virtual void StartDataRequest(const std::string& path,
                                bool is_off_the_record,
                                int request_id);

  virtual std::string GetMimeType(const std::string& path) const;

  static const char kInvalidPasswordHelpUrl[];
  static const char kCanNotAccessAccountUrl[];
  static const char kCreateNewAccountUrl[];

 private:
  virtual ~RemotingResourcesSource() {}

  // Takes a string containing an URL and returns an URL containing a CGI
  // parameter of the form "&hl=xy" where 'xy' is the language code of the
  // current locale.
  std::string GetLocalizedUrl(const std::string& url) const;

  DISALLOW_COPY_AND_ASSIGN(RemotingResourcesSource);
};
