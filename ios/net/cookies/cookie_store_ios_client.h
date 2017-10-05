// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_NET_COOKIES_COOKIE_STORE_IOS_CLIENT_H_
#define IOS_NET_COOKIES_COOKIE_STORE_IOS_CLIENT_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"

namespace net {

class CookieStoreIOSClient;

// Setter and getter for the client.
void SetCookieStoreIOSClient(CookieStoreIOSClient* client);
CookieStoreIOSClient* GetCookieStoreIOSClient();

// Interface that the embedder of the net layer implements. This class lives on
// the same thread as the CookieStoreIOS.
class CookieStoreIOSClient {
 public:
  CookieStoreIOSClient();
  virtual ~CookieStoreIOSClient();

  // Gives the embedder a chance to perform tasks before the cookie storage is
  // changed.
  virtual void WillChangeCookieStorage() const;

  // Informs the embedder after the cookie storage has been changed.
  virtual void DidChangeCookieStorage() const;

  // Returns instance of SequencedTaskRunner used for blocking file I/O.
  virtual scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(CookieStoreIOSClient);
};

}  // namespace net

#endif  // IOS_NET_COOKIES_COOKIE_STORE_IOS_CLIENT_H_
