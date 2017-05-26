// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_CHANNEL_SERVICE_H_
#define EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_CHANNEL_SERVICE_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/cast_channel/cast_socket.h"

namespace extensions {
namespace api {
namespace cast_channel {

// This class adds, removes, and returns cast sockets created by CastChannelAPI
// to underlying storage.
// This class is not thread safe. All methods must be called from the IO thread.
class CastSocketRegistry {
 public:
  CastSocketRegistry();
  ~CastSocketRegistry();

  // Adds |socket| to |sockets_| and returns the new channel_id. Takes ownership
  // of |socket|.
  int AddSocket(std::unique_ptr<CastSocket> socket);

  // Removes the CastSocket corresponding to |channel_id| from the
  // CastSocketRegistry. Returns nullptr if no such CastSocket exists.
  std::unique_ptr<CastSocket> RemoveSocket(int channel_id);

  // Returns the socket corresponding to |channel_id| if one exists, or nullptr
  // otherwise.
  CastSocket* GetSocket(int channel_id) const;

 private:
  // Used to generate CastSocket id.
  static int last_channel_id_;

  // The collection of CastSocket keyed by channel_id.
  std::map<int, std::unique_ptr<CastSocket>> sockets_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(CastSocketRegistry);
};

// This class associates underlying CastSocketRegistry instance with
// BrowserContext and makes sure CastSocketRegistry instance is created and
// destroyed on the IO thread.
// Instance of this class is created and destroyed on the UI thread.
class CastSocketService : public KeyedService {
 public:
  CastSocketService();
  ~CastSocketService() override;

  // Creates CastSocketRegistry instance if none exists. Should be called on the
  // IO thread.
  CastSocketRegistry* GetOrCreateSocketRegistry();

 private:
  std::unique_ptr<CastSocketRegistry, content::BrowserThread::DeleteOnIOThread>
      sockets_;

  DISALLOW_COPY_AND_ASSIGN(CastSocketService);
};

}  // namespace cast_channel
}  // namespace api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_CHANNEL_SERVICE_H_
