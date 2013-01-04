// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SESSION_FAVICON_SOURCE_H_
#define CHROME_BROWSER_UI_WEBUI_SESSION_FAVICON_SOURCE_H_

#include "chrome/browser/ui/webui/favicon_source.h"

class Profile;

namespace browser_sync {
class SessionModelAssociator;
}

// Provides a way to fetch a favicon for a synced tab via a network request.
class SessionFaviconSource : public FaviconSource {
 public:
  explicit SessionFaviconSource(Profile* profile);

  // FaviconSource implementation.
  virtual std::string GetMimeType(const std::string&) const OVERRIDE;
  virtual bool ShouldReplaceExistingSource() const OVERRIDE;
  virtual bool AllowCaching() const OVERRIDE;

 protected:
  virtual ~SessionFaviconSource();
  virtual bool HandleMissingResource(const IconRequest& request) OVERRIDE;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SESSION_FAVICON_SOURCE_H_
