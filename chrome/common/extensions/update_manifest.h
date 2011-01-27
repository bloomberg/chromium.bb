// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_UPDATE_MANIFEST_H_
#define CHROME_COMMON_EXTENSIONS_UPDATE_MANIFEST_H_
#pragma once

#include <string>
#include <vector>

#include "googleurl/src/gurl.h"

class UpdateManifest {
 public:

  // An update manifest looks like this:
  //
  // <?xml version='1.0' encoding='UTF-8'?>
  // <gupdate xmlns='http://www.google.com/update2/response' protocol='2.0'>
  //  <daystart elapsed_seconds='300' />
  //  <app appid='12345'>
  //   <updatecheck codebase='http://example.com/extension_1.2.3.4.crx'
  //                version='1.2.3.4' prodversionmin='2.0.143.0'
  //                hash="12345"/>
  //  </app>
  // </gupdate>
  //
  // The <daystart> tag contains a "elapsed_seconds" attribute which refers to
  // the server's notion of how many seconds it has been since midnight.
  //
  // The "appid" attribute of the <app> tag refers to the unique id of the
  // extension. The "codebase" attribute of the <updatecheck> tag is the url to
  // fetch the updated crx file, and the "prodversionmin" attribute refers to
  // the minimum version of the chrome browser that the update applies to.

  // The result of parsing one <app> tag in an xml update check manifest.
  struct Result {
    Result();
    ~Result();

    std::string extension_id;
    std::string version;
    std::string browser_min_version;
    std::string package_hash;
    GURL crx_url;
  };

  static const int kNoDaystart = -1;
  struct Results {
    Results();
    ~Results();

    std::vector<Result> list;
    // This will be >= 0, or kNoDaystart if the <daystart> tag was not present.
    int daystart_elapsed_seconds;
  };

  UpdateManifest();
  ~UpdateManifest();

  // Parses an update manifest xml string into Result data. Returns a bool
  // indicating success or failure. On success, the results are available by
  // calling results(). The details for any failures are available by calling
  // errors().
  bool Parse(const std::string& manifest_xml);

  const Results& results() { return results_; }
  const std::string& errors() { return errors_; }

 private:
  Results results_;
  std::string errors_;

  // Helper function that adds parse error details to our errors_ string.
  void ParseError(const char* details, ...);

  DISALLOW_COPY_AND_ASSIGN(UpdateManifest);
};

#endif  // CHROME_COMMON_EXTENSIONS_UPDATE_MANIFEST_H_
