// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_SETUP_SOURCE_H_
#define CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_SETUP_SOURCE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/public/browser/url_data_source.h"

class CloudPrintSetupSource : public content::URLDataSource {
 public:
  CloudPrintSetupSource();

  // content::URLDataSource implementation.
  virtual std::string GetSource() OVERRIDE;
  virtual void StartDataRequest(
      const std::string& path,
      bool is_incognito,
      const content::URLDataSource::GotDataCallback& callback) OVERRIDE;
  virtual std::string GetMimeType(const std::string& path) const OVERRIDE;

  static const char kInvalidPasswordHelpUrl[];
  static const char kCanNotAccessAccountUrl[];
  static const char kCreateNewAccountUrl[];

 private:
  virtual ~CloudPrintSetupSource() {}

  // Takes a string containing an URL and returns an URL containing a CGI
  // parameter of the form "&hl=xy" where 'xy' is the language code of the
  // current locale.
  std::string GetLocalizedUrl(const std::string& url) const;

  DISALLOW_COPY_AND_ASSIGN(CloudPrintSetupSource);
};

#endif  // CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_SETUP_SOURCE_H_
