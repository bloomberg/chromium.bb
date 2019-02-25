// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBSHARE_WEBSHARE_TARGET_H_
#define CHROME_BROWSER_WEBSHARE_WEBSHARE_TARGET_H_

#include <string>
#include <vector>

#include "url/gurl.h"

// Represents a ShareTargetFiles entry of a Web Share Target. The attributes are
// usually retrieved from share_target.params.files in the site's manifest.
// https://wicg.github.io/web-share-target/level-2/#sharetargetfiles-and-its-members
class WebShareTargetFiles {
 public:
  WebShareTargetFiles();

  WebShareTargetFiles(const std::string& name,
                      const std::vector<std::string>& accept);

  ~WebShareTargetFiles();

  // Move constructor
  WebShareTargetFiles(WebShareTargetFiles&& other);

  // Move assignment
  WebShareTargetFiles& operator=(WebShareTargetFiles&& other) = default;

  const std::string& name() const { return name_; }
  const std::vector<std::string>& accept() const { return accept_; }

  bool operator==(const WebShareTargetFiles& other) const;

 private:
  std::string name_;
  std::vector<std::string> accept_;

  DISALLOW_COPY_AND_ASSIGN(WebShareTargetFiles);
};

// Used by gtest to print a readable output on test failures.
std::ostream& operator<<(std::ostream& out, const WebShareTargetFiles& target);

// Represents a Web Share Target and its attributes. The attributes are usually
// retrieved from the share_target field in the site's manifest.
// https://wicg.github.io/web-share-target/level-2/#sharetarget-and-its-members
// https://wicg.github.io/web-share-target/level-2/#sharetargetparams-and-its-members
class WebShareTarget {
 public:
  WebShareTarget(const GURL& manifest_url,
                 const std::string& name,
                 const GURL& action,
                 const std::string& method,
                 const std::string& enctype,
                 const std::string& text,
                 const std::string& title,
                 const std::string& url,
                 std::vector<WebShareTargetFiles> files);
  ~WebShareTarget();

  // Move constructor
  WebShareTarget(WebShareTarget&& other);

  // Move assignment
  WebShareTarget& operator=(WebShareTarget&& other) = default;

  const std::string& name() const { return name_; }
  const GURL& manifest_url() const { return manifest_url_; }
  // The action URL to append query parameters to.
  const GURL& action() const { return action_; }
  const std::string& method() const { return method_; }
  const std::string& enctype() const { return enctype_; }

  // Parameters
  const std::string& text() const { return text_; }
  const std::string& title() const { return title_; }
  const std::string& url() const { return url_; }
  const std::vector<WebShareTargetFiles>& files() const { return files_; }

  bool operator==(const WebShareTarget& other) const;

 private:
  GURL manifest_url_;
  std::string name_;
  GURL action_;
  std::string method_;
  std::string enctype_;
  std::string text_;
  std::string title_;
  std::string url_;
  std::vector<WebShareTargetFiles> files_;

  DISALLOW_COPY_AND_ASSIGN(WebShareTarget);
};

// Used by gtest to print a readable output on test failures.
std::ostream& operator<<(std::ostream& out, const WebShareTarget& target);

#endif  // CHROME_BROWSER_WEBSHARE_WEBSHARE_TARGET_H_
