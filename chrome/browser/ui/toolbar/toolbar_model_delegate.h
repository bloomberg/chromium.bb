// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MODEL_DELEGATE_H_
#define CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MODEL_DELEGATE_H_

#include <string>

#include "base/strings/string16.h"

class GURL;

namespace content {
class WebContents;
}

// Delegate which is used by ToolbarModel class.
class ToolbarModelDelegate {
 public:
  // Returns active WebContents.
  virtual content::WebContents* GetActiveWebContents() const = 0;

  // Returns the value to use for the Accept-Languages HTTP header when making
  // an HTTP request.
  virtual std::string GetAcceptLanguages() const = 0;

  // Formats |url| using AutocompleteInput::FormattedStringWithEquivalentMeaning
  // providing an appropriate AutocompleteSchemeClassifier for the embedder.
  virtual base::string16 FormattedStringWithEquivalentMeaning(
      const GURL& url,
      const base::string16& formatted_url) const = 0;

 protected:
  virtual ~ToolbarModelDelegate() {}
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MODEL_DELEGATE_H_
