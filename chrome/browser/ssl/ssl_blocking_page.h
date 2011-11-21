// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_BLOCKING_PAGE_H_
#define CHROME_BROWSER_SSL_SSL_BLOCKING_PAGE_H_
#pragma once

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/string16.h"
#include "chrome/browser/tab_contents/chrome_interstitial_page.h"

class SSLCertErrorHandler;

namespace base {
class DictionaryValue;
}

// This class is responsible for showing/hiding the interstitial page that is
// shown when a certificate error happens.
// It deletes itself when the interstitial page is closed.
class SSLBlockingPage : public ChromeInterstitialPage {
 public:
  SSLBlockingPage(
      SSLCertErrorHandler* handler,
      bool overridable,
      const base::Callback<void(SSLCertErrorHandler*, bool)>& callback);
  virtual ~SSLBlockingPage();

  // A method that sets strings in the specified dictionary from the passed
  // vector so that they can be used to resource the ssl_roadblock.html/
  // ssl_error.html files.
  // Note: there can be up to 5 strings in |extra_info|.
  static void SetExtraInfo(base::DictionaryValue* strings,
                           const std::vector<string16>& extra_info);

 protected:
  // ChromeInterstitialPage implementation.
  virtual std::string GetHTMLContents() OVERRIDE;
  virtual void CommandReceived(const std::string& command) OVERRIDE;
  virtual void UpdateEntry(NavigationEntry* entry) OVERRIDE;
  virtual void Proceed() OVERRIDE;
  virtual void DontProceed() OVERRIDE;

 private:
  void NotifyDenyCertificate();
  void NotifyAllowCertificate();

  // The error we represent.  We will either call CancelRequest() or
  // ContinueRequest() on this object.
  scoped_refptr<SSLCertErrorHandler> handler_;

  base::Callback<void(SSLCertErrorHandler*, bool)> callback_;

  // Is the certificate error overridable or fatal?
  bool overridable_;

  DISALLOW_COPY_AND_ASSIGN(SSLBlockingPage);
};

#endif  // CHROME_BROWSER_SSL_SSL_BLOCKING_PAGE_H_
