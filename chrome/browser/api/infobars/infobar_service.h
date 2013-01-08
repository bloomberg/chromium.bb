// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_API_INFOBARS_INFOBAR_SERVICE_H_
#define CHROME_BROWSER_API_INFOBARS_INFOBAR_SERVICE_H_

#include "base/memory/scoped_ptr.h"

namespace content {
class WebContents;
}

class InfoBarDelegate;

// Provides access to creating, removing and enumerating info bars
// attached to a tab.
class InfoBarService {
 public:
  // Passthrough functions to the implementing subclass.  The subclass .cc file
  // should define these.
  static void CreateForWebContents(content::WebContents* web_contents);
  static InfoBarService* FromWebContents(content::WebContents* web_contents);
  static const InfoBarService*
      FromWebContents(const content::WebContents* web_contents);

  virtual ~InfoBarService();

  // Changes whether infobars are enabled.  The default is true.
  virtual void SetInfoBarsEnabled(bool enabled) = 0;

  // Adds an InfoBar for the specified |delegate|.
  //
  // If infobars are disabled for this tab or the tab already has a delegate
  // which returns true for InfoBarDelegate::EqualsDelegate(delegate),
  // |delegate| is closed immediately without being added.
  //
  // Returns the delegate if it was successfully added.
  virtual InfoBarDelegate* AddInfoBar(scoped_ptr<InfoBarDelegate> delegate) = 0;

  // Removes the InfoBar for the specified |delegate|.
  //
  // If infobars are disabled for this tab, this will do nothing, on the
  // assumption that the matching AddInfoBar() call will have already closed the
  // delegate (see above).
  virtual void RemoveInfoBar(InfoBarDelegate* delegate) = 0;

  // Replaces one infobar with another, without any animation in between.
  //
  // If infobars are disabled for this tab, |new_delegate| is closed immediately
  // without being added, and nothing else happens.
  //
  // Returns the new delegate if it was successfully added.
  //
  // NOTE: This does not perform any EqualsDelegate() checks like AddInfoBar().
  virtual InfoBarDelegate* ReplaceInfoBar(
      InfoBarDelegate* old_delegate,
      scoped_ptr<InfoBarDelegate> new_delegate) = 0;

  // Returns the number of infobars for this tab.
  virtual size_t GetInfoBarCount() const = 0;

  // Returns the infobar delegate at the given |index|.  The InfoBarService
  // retains ownership.
  //
  // Warning: Does not sanity check |index|.
  virtual InfoBarDelegate* GetInfoBarDelegateAt(size_t index) = 0;

  // Retrieve the WebContents for the tab this service is associated with.
  virtual content::WebContents* GetWebContents() = 0;
};

#endif  // CHROME_BROWSER_API_INFOBARS_INFOBAR_SERVICE_H_
