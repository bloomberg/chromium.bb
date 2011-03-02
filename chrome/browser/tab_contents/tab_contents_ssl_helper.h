// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_SSL_HELPER_H_
#define CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_SSL_HELPER_H_
#pragma once

#include <map>

#include "base/linked_ptr.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"

class SSLAddCertHandler;
class SSLClientAuthHandler;
class TabContents;

class TabContentsSSLHelper : public RenderViewHostDelegate::SSL {
 public:
  explicit TabContentsSSLHelper(TabContents* tab_contents);
  virtual ~TabContentsSSLHelper();

  // RenderViewHostDelegate::SSL implementation:
  virtual void ShowClientCertificateRequestDialog(
      scoped_refptr<SSLClientAuthHandler> handler);
  virtual void OnVerifyClientCertificateError(
      scoped_refptr<SSLAddCertHandler> handler, int error_code);
  virtual void AskToAddClientCertificate(
      scoped_refptr<SSLAddCertHandler> handler);
  virtual void OnAddClientCertificateSuccess(
      scoped_refptr<SSLAddCertHandler> handler);
  virtual void OnAddClientCertificateError(
      scoped_refptr<SSLAddCertHandler> handler, int error_code);
  virtual void OnAddClientCertificateFinished(
      scoped_refptr<SSLAddCertHandler> handler);

 private:
  TabContents* tab_contents_;

  class SSLAddCertData;
  std::map<int, linked_ptr<SSLAddCertData> > request_id_to_add_cert_data_;

  SSLAddCertData* GetAddCertData(SSLAddCertHandler* handler);

  DISALLOW_COPY_AND_ASSIGN(TabContentsSSLHelper);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_SSL_HELPER_H_
