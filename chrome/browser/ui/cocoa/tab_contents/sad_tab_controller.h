// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_SAD_TAB_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_SAD_TAB_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/sad_tab.h"

#import <Cocoa/Cocoa.h>

@class SadTabController;
@class SadTabView;

namespace chrome {

class SadTabCocoa : public SadTab {
 public:
  explicit SadTabCocoa(content::WebContents* web_contents);

  ~SadTabCocoa() override;

 private:
  // Overridden from SadTab:
  void Show() override;
  void Close() override;

  base::scoped_nsobject<SadTabController> sad_tab_controller_;

  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(SadTabCocoa);
};

}  // namespace chrome

// A controller class that manages the SadTabView (aka "Aw Snap" or crash page).
@interface SadTabController : NSViewController {
 @private
  content::WebContents* webContents_;  // Weak reference.
  base::scoped_nsobject<SadTabView> sadTabView_;
}

// Designated initializer.
- (id)initWithWebContents:(content::WebContents*)webContents;

// Returns a weak reference to the WebContents whose WebContentsView created
// this SadTabController.
- (content::WebContents*)webContents;

@end

#endif  // CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_SAD_TAB_CONTROLLER_H_
