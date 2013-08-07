// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_WEBRTC_IDENTITY_STORE_H_
#define CONTENT_BROWSER_MEDIA_WEBRTC_IDENTITY_STORE_H_

#include <string>

#include "base/callback.h"
#include "content/common/content_export.h"

class GURL;

namespace base {
class TaskRunner;
}  // namespace base

namespace content {

class WebRTCIdentityStoreTest;

// A class for creating and fetching DTLS identities, i.e. the private key and
// the self-signed certificate.
class CONTENT_EXPORT WebRTCIdentityStore {
 public:
  typedef base::Callback<void(int error,
                              const std::string& certificate,
                              const std::string& private_key)>
      CompletionCallback;

  WebRTCIdentityStore();
  // Only virtual to allow subclassing for test mock.
  virtual ~WebRTCIdentityStore();

  // Retrieve the cached DTLS private key and certificate, i.e. identity, for
  // the |origin| and |identity_name| pair, or generate a new identity using
  // |common_name| if such an identity does not exist.
  // If the given |common_name| is different from the common name in the cached
  // identity that has the same origin and identity_name, a new private key and
  // a new certificate will be generated, overwriting the old one.
  // TODO(jiayl): implement identity caching through a persistent storage.
  //
  // |origin| is the origin of the DTLS connection;
  // |identity_name| is used to identify an identity within an origin; it is
  // opaque to WebRTCIdentityStore and remains private to the caller, i.e. not
  // present in the certificate;
  // |common_name| is the common name used to generate the certificate and will
  // be shared with the peer of the DTLS connection. Identities created for
  // different origins or different identity names may have the same common
  // name.
  // |callback| is the callback to return the result.
  //
  // Returns the Closure used to cancel the request if the request is accepted.
  // The Closure can only be called before the request completes.
  virtual base::Closure RequestIdentity(const GURL& origin,
                                        const std::string& identity_name,
                                        const std::string& common_name,
                                        const CompletionCallback& callback);

 private:
  friend class WebRTCIdentityStoreTest;

  void SetTaskRunnerForTesting(
      const scoped_refptr<base::TaskRunner>& task_runner);

  // The TaskRunner for doing work on a worker thread.
  scoped_refptr<base::TaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(WebRTCIdentityStore);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_WEBRTC_IDENTITY_STORE_H_
