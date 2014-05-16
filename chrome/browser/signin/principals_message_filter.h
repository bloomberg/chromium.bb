// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_PRINCIPALS_MESSAGE_FILTER_H_
#define CHROME_BROWSER_SIGNIN_PRINCIPALS_MESSAGE_FILTER_H_

#include "content/public/browser/browser_message_filter.h"

class GURL;
class Profile;

// A message filter implementation that receives messages for browser account
// management from the renderer.
class PrincipalsMessageFilter : public content::BrowserMessageFilter {
 public:
  explicit PrincipalsMessageFilter(int render_process_id);

  // content::BrowserMessageFilter implementation.
  virtual void OverrideThreadForMessage(
      const IPC::Message& message,
      content::BrowserThread::ID* thread) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  virtual ~PrincipalsMessageFilter();

  void OnMsgShowBrowserAccountManagementUI();
  void OnMsgGetManagedAccounts(const GURL& url,
                               std::vector<std::string>* managed_accounts);

  int render_process_id_;

  DISALLOW_COPY_AND_ASSIGN(PrincipalsMessageFilter);
};

#endif  // CHROME_BROWSER_SIGNIN_PRINCIPALS_MESSAGE_FILTER_H_
