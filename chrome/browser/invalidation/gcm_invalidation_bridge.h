// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INVALIDATION_GCM_INVALIDATION_BRIDGE_H_
#define CHROME_BROWSER_INVALIDATION_GCM_INVALIDATION_BRIDGE_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/services/gcm/gcm_event_router.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "google_apis/gcm/gcm_client.h"
#include "sync/notifier/gcm_network_channel_delegate.h"

class Profile;

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace invalidation {

// GCMInvalidationBridge and GCMInvalidationBridge::Core implement functions
// needed for GCMNetworkChannel. GCMInvalidationBridge lives on UI thread while
// Core lives on IO thread. Core implements GCMNetworkChannelDelegate and posts
// all function calls to GCMInvalidationBridge which does actual work to perform
// them.
class GCMInvalidationBridge : public gcm::GCMEventRouter,
                              public OAuth2TokenService::Consumer,
                              public base::NonThreadSafe {
 public:
  class Core;

  explicit GCMInvalidationBridge(Profile* profile);
  virtual ~GCMInvalidationBridge();

  // OAuth2TokenService::Consumer implementation.
  virtual void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                                 const std::string& access_token,
                                 const base::Time& expiration_time) OVERRIDE;
  virtual void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                                 const GoogleServiceAuthError& error) OVERRIDE;

  // gcm::GCMEventRouter implementation.
  virtual void OnMessage(const std::string& app_id,
                         const gcm::GCMClient::IncomingMessage& message)
      OVERRIDE;
  virtual void OnMessagesDeleted(const std::string& app_id) OVERRIDE;
  virtual void OnSendError(
      const std::string& app_id,
      const gcm::GCMClient::SendErrorDetails& send_error_details) OVERRIDE;

  scoped_ptr<syncer::GCMNetworkChannelDelegate> CreateDelegate();

  void CoreInitializationDone(
      base::WeakPtr<Core> core,
      scoped_refptr<base::SingleThreadTaskRunner> core_thread_task_runner);

  // Functions reflecting GCMNetworkChannelDelegate interface. These are called
  // on UI thread to perform actual work.
  void RequestToken(
      syncer::GCMNetworkChannelDelegate::RequestTokenCallback callback);
  void InvalidateToken(const std::string& token);

  void Register(syncer::GCMNetworkChannelDelegate::RegisterCallback callback);

  void SubscribeForIncomingMessages();

  void RegisterFinished(
      syncer::GCMNetworkChannelDelegate::RegisterCallback callback,
      const std::string& registration_id,
      gcm::GCMClient::Result result);

 private:
  // GCMInvalidationBridge is owned by TiclInvalidationService therefore it is
  // expected that profile_ pointer is valid throughout lifetime of this object.
  Profile* profile_;

  base::WeakPtr<Core> core_;
  scoped_refptr<base::SingleThreadTaskRunner> core_thread_task_runner_;

  // Fields related to RequestToken function.
  scoped_ptr<OAuth2TokenService::Request> access_token_request_;
  syncer::GCMNetworkChannelDelegate::RequestTokenCallback
      request_token_callback_;
  bool subscribed_for_incoming_messages_;

  base::WeakPtrFactory<GCMInvalidationBridge> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GCMInvalidationBridge);
};

}  // namespace invalidation

#endif  // CHROME_BROWSER_INVALIDATION_GCM_INVALIDATION_BRIDGE_H_
