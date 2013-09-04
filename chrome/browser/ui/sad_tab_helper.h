// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAD_TAB_HELPER_H_
#define CHROME_BROWSER_UI_SAD_TAB_HELPER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace chrome {
class SadTab;
}

// Per-tab class to manage sad tab views.
class SadTabHelper : public content::WebContentsObserver,
                     public content::WebContentsUserData<SadTabHelper> {
 public:
  virtual ~SadTabHelper();

  chrome::SadTab* sad_tab() { return sad_tab_.get(); }

 private:
  friend class content::WebContentsUserData<SadTabHelper>;

  explicit SadTabHelper(content::WebContents* web_contents);

  void InstallSadTab(base::TerminationStatus status);

  // Overridden from content::WebContentsObserver:
  virtual void RenderViewReady() OVERRIDE;
  virtual void RenderProcessGone(base::TerminationStatus status) OVERRIDE;

  scoped_ptr<chrome::SadTab> sad_tab_;

  DISALLOW_COPY_AND_ASSIGN(SadTabHelper);
};

#endif  // CHROME_BROWSER_UI_SAD_TAB_HELPER_H_
