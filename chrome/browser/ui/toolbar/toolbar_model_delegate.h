// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MODEL_DELEGATE_H_
#define CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MODEL_DELEGATE_H_

namespace content {
class WebContents;
}

// Delegate which is used by ToolbarModel class.
class ToolbarModelDelegate {
 public:
  // Returns active WebContents.
  virtual content::WebContents* GetActiveWebContents() const = 0;

 protected:
  virtual ~ToolbarModelDelegate() {}
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_MODEL_DELEGATE_H_
