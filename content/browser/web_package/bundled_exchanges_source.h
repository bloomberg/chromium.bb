// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_SOURCE_H_
#define CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_SOURCE_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

// A class to abstract required information to access a BundledExchanges.
class CONTENT_EXPORT BundledExchangesSource {
 public:
  static std::unique_ptr<BundledExchangesSource> CreateFromTrustedFile(
      const base::FilePath& file_path);

  std::unique_ptr<BundledExchangesSource> Clone() const;

  ~BundledExchangesSource() = default;

  // A flag to represent if this source can be trusted, i.e. using the URL in
  // the BundledExchanges as the origin for the content. Otherwise, we will use
  // the origin that serves the BundledExchanges itself. For instance, if the
  // BundledExchanges is in a local file system, file:// should be the origin.
  //
  // Currently this flag is always true because we only support
  // trustable-bundled-exchanges-file loading. TODO(horo): Implement
  // untrusted bundled exchanges file loading.
  bool is_trusted() const { return is_trusted_; }

  const base::FilePath& file_path() const { return file_path_; }
  const GURL& url() const { return url_; }

 private:
  BundledExchangesSource(bool is_trusted,
                         const base::FilePath& file_path,
                         const GURL& url);

  const bool is_trusted_;
  const base::FilePath file_path_;
  const GURL url_;

  DISALLOW_COPY_AND_ASSIGN(BundledExchangesSource);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_BUNDLED_EXCHANGES_SOURCE_H_
