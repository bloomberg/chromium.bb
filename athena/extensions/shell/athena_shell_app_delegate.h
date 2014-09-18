// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_EXTENSIONS_SHELL_ATHENA_APP_DELEGATE_H_
#define ATHENA_EXTENSIONS_SHELL_ATHENA_APP_DELEGATE_H_

#include "athena/extensions/athena_app_delegate_base.h"

namespace athena {

class AthenaShellAppDelegate : public AthenaAppDelegateBase {
 public:
  AthenaShellAppDelegate();
  virtual ~AthenaShellAppDelegate();

 private:
  // extensions::AppDelegate:
  virtual void InitWebContents(content::WebContents* web_contents) OVERRIDE;
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
  virtual bool CheckMediaAccessPermission(
      content::WebContents* web_contents,
      const GURL& security_origin,
      content::MediaStreamType type,
      const extensions::Extension* extension) OVERRIDE;
  virtual void SetWebContentsBlocked(content::WebContents* web_contents,
                                     bool blocked) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(AthenaShellAppDelegate);
};

}  // namespace athena

#endif  // ATHENA_EXTENSIONS_SHELL_ATHENA_APP_DELEGATE_H_
