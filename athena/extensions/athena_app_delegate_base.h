// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_EXTENSIONS_ATHENA_APP_DELEGATE_BASE_H_
#define ATHENA_EXTENSIONS_ATHENA_APP_DELEGATE_BASE_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "extensions/browser/app_window/app_delegate.h"

namespace athena {

class AthenaAppDelegateBase : public extensions::AppDelegate {
 public:
  AthenaAppDelegateBase();
  virtual ~AthenaAppDelegateBase();

 private:
  class NewActivityContentsDelegate;

  // extensions::AppDelegate:
  virtual void ResizeWebContents(content::WebContents* web_contents,
                                 const gfx::Size& size) OVERRIDE;
  virtual content::WebContents* OpenURLFromTab(
      content::BrowserContext* context,
      content::WebContents* source,
      const content::OpenURLParams& params) OVERRIDE;
  virtual void AddNewContents(content::BrowserContext* context,
                              content::WebContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture,
                              bool* was_blocked) OVERRIDE;
  virtual int PreferredIconSize() OVERRIDE;
  virtual bool IsWebContentsVisible(
      content::WebContents* web_contents) OVERRIDE;
  virtual void SetTerminatingCallback(const base::Closure& callback) OVERRIDE;

  scoped_ptr<NewActivityContentsDelegate> new_window_contents_delegate_;
  base::Closure terminating_callback_;

  DISALLOW_COPY_AND_ASSIGN(AthenaAppDelegateBase);
};

}  // namespace athena

#endif  // ATHENA_EXTENSIONS_ATHENA_APP_DELEGATE_BASE_H_
