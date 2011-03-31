// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ENGINE_HTTP_POST_PROVIDER_FACTORY_H_
#define CHROME_BROWSER_SYNC_ENGINE_HTTP_POST_PROVIDER_FACTORY_H_
#pragma once

namespace sync_api {

class HttpPostProviderInterface;

// A factory to create HttpPostProviders to hide details about the
// implementations and dependencies.
// A factory instance itself should be owned by whomever uses it to create
// HttpPostProviders.
class HttpPostProviderFactory {
 public:
  virtual ~HttpPostProviderFactory() {}

  // Obtain a new HttpPostProviderInterface instance, owned by caller.
  virtual HttpPostProviderInterface* Create() = 0;

  // When the interface is no longer needed (ready to be cleaned up), clients
  // must call Destroy().
  // This allows actual HttpPostProvider subclass implementations to be
  // reference counted, which is useful if a particular implementation uses
  // multiple threads to serve network requests.
  virtual void Destroy(HttpPostProviderInterface* http) = 0;
};

}  // namespace sync_api

#endif  // CHROME_BROWSER_SYNC_ENGINE_HTTP_POST_PROVIDER_FACTORY_H_
