// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_SSL_HELPER_H_
#define CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_SSL_HELPER_H_

#include "chrome/browser/renderer_host/render_view_host_delegate.h"

class SSLClientAuthHandler;
class TabContents;

class TabContentsSSLHelper : public RenderViewHostDelegate::SSL {
 public:
  explicit TabContentsSSLHelper(TabContents* tab_contents);
  virtual ~TabContentsSSLHelper();

  // RenderViewHostDelegate::SSL implementation:
  virtual void ShowClientCertificateRequestDialog(
      scoped_refptr<SSLClientAuthHandler> handler);

 private:
  TabContents* tab_contents_;

  DISALLOW_COPY_AND_ASSIGN(TabContentsSSLHelper);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_SSL_HELPER_H_
