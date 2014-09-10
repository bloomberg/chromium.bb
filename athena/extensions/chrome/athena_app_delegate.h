// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_EXTENSIONS_CHROME_ATHENA_APP_DELEGATE_H_
#define ATHENA_EXTENSIONS_CHROME_ATHENA_APP_DELEGATE_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "extensions/browser/app_window/app_delegate.h"
#include "ui/base/window_open_disposition.h"

namespace athena {

class AthenaAppDelegate : public extensions::AppDelegate {
 public:
  AthenaAppDelegate();
  virtual ~AthenaAppDelegate();

 private:
  class NewWindowContentsDelegate;

  // extensions::AppDelegate:
  virtual void InitWebContents(content::WebContents* web_contents) OVERRIDE;
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
  virtual content::ColorChooser* ShowColorChooser(
      content::WebContents* web_contents,
      SkColor initial_color) OVERRIDE;
  virtual void RunFileChooser(
      content::WebContents* tab,
      const content::FileChooserParams& params) OVERRIDE;
  virtual void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback,
      const extensions::Extension* extension) OVERRIDE;
  virtual int PreferredIconSize() OVERRIDE;
  virtual gfx::ImageSkia GetAppDefaultIcon() OVERRIDE;
  virtual void SetWebContentsBlocked(content::WebContents* web_contents,
                                     bool blocked) OVERRIDE;
  virtual bool IsWebContentsVisible(
      content::WebContents* web_contents) OVERRIDE;
  virtual void SetTerminatingCallback(const base::Closure& callback) OVERRIDE;

  scoped_ptr<NewWindowContentsDelegate> new_window_contents_delegate_;
  base::Closure terminating_callback_;

  DISALLOW_COPY_AND_ASSIGN(AthenaAppDelegate);
};

}  // namespace athena

#endif  // ATHENA_EXTENSIONS_CHROME_ATHENA_APP_DELEGATE_H_
