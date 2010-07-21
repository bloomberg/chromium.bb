// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/tab_contents_ssl_helper.h"

#include "chrome/browser/ssl/ssl_client_auth_handler.h"
#include "chrome/browser/ssl_client_certificate_selector.h"
#include "chrome/browser/tab_contents/tab_contents.h"

TabContentsSSLHelper::TabContentsSSLHelper(TabContents* tab_contents)
    : tab_contents_(tab_contents) {
}

TabContentsSSLHelper::~TabContentsSSLHelper() {
}

void TabContentsSSLHelper::ShowClientCertificateRequestDialog(
    scoped_refptr<SSLClientAuthHandler> handler) {
  // Display the native certificate request dialog for each platform.
  browser::ShowSSLClientCertificateSelector(
      tab_contents_->GetMessageBoxRootWindow(),
      handler->cert_request_info(), handler);
}
