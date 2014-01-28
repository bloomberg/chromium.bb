// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/nss_context.h"

#include "base/message_loop/message_loop_proxy.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_context.h"

using content::BrowserThread;

namespace {

// Relays callback to the right message loop.
void DidGetCertDBOnIOThread(
    scoped_refptr<base::MessageLoopProxy> response_message_loop,
    const base::Callback<void(net::NSSCertDatabase*)>& callback,
    net::NSSCertDatabase* cert_db) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  response_message_loop->PostTask(FROM_HERE, base::Bind(callback, cert_db));
}

// Gets NSSCertDatabase for the resource context.
void GetCertDBOnIOThread(
    content::ResourceContext* context,
    scoped_refptr<base::MessageLoopProxy> response_message_loop,
    const base::Callback<void(net::NSSCertDatabase*)>& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Note that the callback will be used only if the cert database hasn't yet
  // been initialized.
  net::NSSCertDatabase* cert_db = GetNSSCertDatabaseForResourceContext(
      context,
      base::Bind(&DidGetCertDBOnIOThread, response_message_loop, callback));

  if (cert_db)
    DidGetCertDBOnIOThread(response_message_loop, callback, cert_db);
}

}  // namespace

void GetNSSCertDatabaseForProfile(
    Profile* profile,
    const base::Callback<void(net::NSSCertDatabase*)>& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BrowserThread::PostTask(BrowserThread::IO,
                          FROM_HERE,
                          base::Bind(&GetCertDBOnIOThread,
                                     profile->GetResourceContext(),
                                     base::MessageLoopProxy::current(),
                                     callback));
}

