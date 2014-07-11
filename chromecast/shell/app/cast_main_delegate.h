// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_SHELL_APP_CAST_MAIN_DELEGATE_H_
#define CHROMECAST_SHELL_APP_CAST_MAIN_DELEGATE_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "chromecast/shell/common/cast_content_client.h"
#include "content/public/app/content_main_delegate.h"

namespace chromecast {
namespace shell {

class CastContentBrowserClient;
class CastContentRendererClient;

class CastMainDelegate : public content::ContentMainDelegate {
 public:
  CastMainDelegate();
  virtual ~CastMainDelegate();

  // content::ContentMainDelegate implementation:
  virtual bool BasicStartupComplete(int* exit_code) OVERRIDE;
  virtual void PreSandboxStartup() OVERRIDE;
  virtual void ZygoteForked() OVERRIDE;
  virtual content::ContentBrowserClient* CreateContentBrowserClient() OVERRIDE;
  virtual content::ContentRendererClient*
      CreateContentRendererClient() OVERRIDE;

 private:
  static void InitializeResourceBundle();

  scoped_ptr<CastContentBrowserClient> browser_client_;
  scoped_ptr<CastContentRendererClient> renderer_client_;
  CastContentClient content_client_;

  DISALLOW_COPY_AND_ASSIGN(CastMainDelegate);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_SHELL_APP_CAST_MAIN_DELEGATE_H_
