// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAVICON_CHROME_FAVICON_CLIENT_H_
#define CHROME_BROWSER_FAVICON_CHROME_FAVICON_CLIENT_H_

#include "components/favicon/core/browser/favicon_client.h"

#include "base/macros.h"

class FaviconService;
class GURL;
class Profile;

// ChromeFaviconClient implements the the FaviconClient interface.
class ChromeFaviconClient : public FaviconClient {
 public:
  explicit ChromeFaviconClient(Profile* profile);
  virtual ~ChromeFaviconClient();

  // FaviconClient implementation:
  virtual FaviconService* GetFaviconService() OVERRIDE;
  virtual bool IsBookmarked(const GURL& url) OVERRIDE;

 private:
  Profile* profile_;
  DISALLOW_COPY_AND_ASSIGN(ChromeFaviconClient);
};

#endif  // CHROME_BROWSER_FAVICON_CHROME_FAVICON_CLIENT_H_
