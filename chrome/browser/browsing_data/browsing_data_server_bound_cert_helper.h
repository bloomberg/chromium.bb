// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_SERVER_BOUND_CERT_HELPER_H_
#define CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_SERVER_BOUND_CERT_HELPER_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "net/ssl/server_bound_cert_store.h"

class Profile;

// BrowsingDataServerBoundCertHelper is an interface for classes dealing with
// aggregating and deleting browsing data stored in the server bound cert store.
// A client of this class need to call StartFetching from the UI thread to
// initiate the flow, and it'll be notified by the callback in its UI thread at
// some later point.
class BrowsingDataServerBoundCertHelper
    : public base::RefCountedThreadSafe<BrowsingDataServerBoundCertHelper> {
 public:

  // Create a BrowsingDataServerBoundCertHelper instance for the given
  // |profile|.
  static BrowsingDataServerBoundCertHelper* Create(Profile* profile);

  typedef base::Callback<
      void(const net::ServerBoundCertStore::ServerBoundCertList&)>
      FetchResultCallback;

  // Starts the fetching process, which will notify its completion via
  // callback.
  // This must be called only in the UI thread.
  virtual void StartFetching(const FetchResultCallback& callback) = 0;
  // Requests a single server bound cert to be deleted.  This must be called in
  // the UI thread.
  virtual void DeleteServerBoundCert(const std::string& server_id) = 0;

 protected:
  friend class base::RefCountedThreadSafe<BrowsingDataServerBoundCertHelper>;
  virtual ~BrowsingDataServerBoundCertHelper() {}
};

// This class is a thin wrapper around BrowsingDataServerBoundCertHelper that
// does not fetch its information from the ServerBoundCertService, but gets them
// passed as a parameter during construction.
class CannedBrowsingDataServerBoundCertHelper
    : public BrowsingDataServerBoundCertHelper {
 public:
  CannedBrowsingDataServerBoundCertHelper();

  // Return a copy of the ServerBoundCert helper. Only one consumer can use the
  // StartFetching method at a time, so we need to create a copy of the helper
  // every time we instantiate a cookies tree model for it.
  CannedBrowsingDataServerBoundCertHelper* Clone();

  // Add an ServerBoundCert to the set of canned server bound certs that is
  // returned by this helper.
  void AddServerBoundCert(
      const net::ServerBoundCertStore::ServerBoundCert& server_bound_cert);

  // Clears the list of canned server bound certs.
  void Reset();

  // True if no ServerBoundCerts are currently stored.
  bool empty() const;

  // Returns the current number of server bound certificates.
  size_t GetCertCount() const;

  // BrowsingDataServerBoundCertHelper methods.
  virtual void StartFetching(const FetchResultCallback& callback) OVERRIDE;
  virtual void DeleteServerBoundCert(const std::string& server_id) OVERRIDE;

 private:
  virtual ~CannedBrowsingDataServerBoundCertHelper();

  void FinishFetching();

  typedef std::map<std::string, net::ServerBoundCertStore::ServerBoundCert>
      ServerBoundCertMap;
  ServerBoundCertMap server_bound_cert_map_;

  FetchResultCallback completion_callback_;

  DISALLOW_COPY_AND_ASSIGN(CannedBrowsingDataServerBoundCertHelper);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_SERVER_BOUND_CERT_HELPER_H_
