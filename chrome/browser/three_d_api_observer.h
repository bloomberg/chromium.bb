// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THREE_D_API_OBSERVER_H_
#define CHROME_BROWSER_THREE_D_API_OBSERVER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class ThreeDAPIObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ThreeDAPIObserver> {
 public:
  virtual ~ThreeDAPIObserver();

 private:
  explicit ThreeDAPIObserver(content::WebContents* web_contents);
  friend class content::WebContentsUserData<ThreeDAPIObserver>;

  virtual void DidBlock3DAPIs(const GURL& url,
                              content::ThreeDAPIType requester) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ThreeDAPIObserver);
};

#endif  // CHROME_BROWSER_THREE_D_API_OBSERVER_H_
