// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPEN_FROM_CLIPBOARD_CLIPBOARD_RECENT_CONTENT_H_
#define COMPONENTS_OPEN_FROM_CLIPBOARD_CLIPBOARD_RECENT_CONTENT_H_

#include <string>

#include "base/macros.h"

class GURL;

// Helper class returning an URL if the content of the clipboard can be turned
// into an URL, and if it estimates that the content of the clipboard is not too
// old.
class ClipboardRecentContent {
 public:
  // Returns an instance of the ClipboardContent singleton.
  static ClipboardRecentContent* GetInstance();

  // Returns true if the clipboard contains a recent URL, and copies it in
  // |url|. Otherwise, returns false. |url| must not be null.
  virtual bool GetRecentURLFromClipboard(GURL* url) const = 0;

  // Sets which URL scheme this app can be opened with. Used by
  // ClipboardRecentContent to determine whether or not the clipboard contains
  // a relevant URL. |application_scheme| may be empty.
  void set_application_scheme(const std::string& application_scheme) {
    application_scheme_ = application_scheme;
  }

 protected:
  ClipboardRecentContent() {}
  virtual ~ClipboardRecentContent() {}
  // Contains the URL scheme opening the app. May be empty.
  std::string application_scheme_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ClipboardRecentContent);
};

#endif  // COMPONENTS_OPEN_FROM_CLIPBOARD_CLIPBOARD_RECENT_CONTENT_H_
