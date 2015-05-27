// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_SHELL_SHELL_BROWSER_STATE_H_
#define IOS_WEB_SHELL_SHELL_BROWSER_STATE_H_

#include "ios/web/public/browser_state.h"

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace web {

class ShellBrowserContext;
class ShellURLRequestContextGetter;

// Shell-specific implementation of BrowserState.  Can only be called from the
// UI thread.
class ShellBrowserState : public BrowserState {
 public:
  ShellBrowserState();
  ~ShellBrowserState() override;

  // BrowserState implementation.
  bool IsOffTheRecord() const override;
  base::FilePath GetStatePath() const override;
  net::URLRequestContextGetter* GetRequestContext() override;

 private:
  base::FilePath path_;
  scoped_refptr<ShellURLRequestContextGetter> request_context_getter_;

  DISALLOW_COPY_AND_ASSIGN(ShellBrowserState);
};

}  // namespace web

#endif  // IOS_WEB_SHELL_SHELL_BROWSER_STATE_H_
