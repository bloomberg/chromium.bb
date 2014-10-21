// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SPY_SPY_SERVER_IMPL_H_
#define MOJO_SPY_SPY_SERVER_IMPL_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/spy/public/spy.mojom.h"
#include "url/gurl.h"

namespace mojo {

class SpyServerImpl :
    public base::RefCounted<SpyServerImpl>,
    public InterfaceImpl<spy_api::SpyServer> {
 public:
  SpyServerImpl();

  // spy_api::SpyServer implementation.
  void StartSession(spy_api::VersionPtr version,
                    const mojo::Callback<void(spy_api::Result, mojo::String)>&
                        callback) override;

  void StopSession(
      const mojo::Callback<void(spy_api::Result)>& callback) override;

  void TrackConnection(
      uint32_t id,
      spy_api::ConnectionOptions options,
      const mojo::Callback<void(spy_api::Result)>& callback) override;

  void OnConnectionError() override;

  // SpyServerImpl own methods.
  void OnIntercept(const GURL& url);

  ScopedMessagePipeHandle ServerPipe();

 private:
  friend class base::RefCounted<SpyServerImpl>;
  ~SpyServerImpl() override;

  // Item models the entities that we track by IDs.
  struct Item;

  MessagePipe pipe_;
  bool has_session_;
  std::map<uint32_t, Item*> items_;
};

}  // namespace mojo

#endif  // MOJO_SPY_SPY_SERVER_IMPL_H_
