// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_COPRESENCE_MANAGER_IMPL_H_
#define COMPONENTS_COPRESENCE_COPRESENCE_MANAGER_IMPL_H_

#include <string>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/copresence/copresence_state_impl.h"
#include "components/copresence/public/copresence_manager.h"

namespace base {
class Timer;
}

namespace net {
class URLContextGetter;
}

namespace copresence {

class DirectiveHandler;
class GCMHandler;
class ReportRequest;
class RpcHandler;
class WhispernetClient;

// The implementation for CopresenceManager. Responsible primarily for
// client-side initialization. The RpcHandler handles all the details
// of interacting with the server.
// TODO(rkc): Add tests for this class.
class CopresenceManagerImpl : public CopresenceManager {
 public:
  // The delegate is owned by the caller, and must outlive the manager.
  explicit CopresenceManagerImpl(CopresenceDelegate* delegate);

  ~CopresenceManagerImpl() override;

  // CopresenceManager overrides.
  CopresenceState* state() override;
  void ExecuteReportRequest(const ReportRequest& request,
                            const std::string& app_id,
                            const std::string& auth_token,
                            const StatusCallback& callback) override;

 private:
  void WhispernetInitComplete(bool success);

  // Handle tokens decoded by Whispernet.
  // TODO(ckehoe): Replace AudioToken with ReceivedToken.
  void ReceivedTokens(const std::vector<AudioToken>& tokens);

  // This function will be called every kPollTimerIntervalMs milliseconds
  // to poll the server for new messages.
  void PollForMessages();

  // Verify that we can hear the audio we're playing
  // every kAudioCheckIntervalMs milliseconds.
  void AudioCheck();

  // Belongs to the caller.
  CopresenceDelegate* const delegate_;

  // We use a CancelableCallback here because Whispernet
  // does not provide a way to unregister its init callback.
  base::CancelableCallback<void(bool)> whispernet_init_callback_;

  bool init_failed_;

  // This order is required because each object
  // makes calls to those listed before it.
  scoped_ptr<CopresenceStateImpl> state_;
  scoped_ptr<RpcHandler> rpc_handler_;
  scoped_ptr<DirectiveHandler> directive_handler_;
  scoped_ptr<GCMHandler> gcm_handler_;

  scoped_ptr<base::Timer> poll_timer_;
  scoped_ptr<base::Timer> audio_check_timer_;

  DISALLOW_COPY_AND_ASSIGN(CopresenceManagerImpl);
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_COPRESENCE_MANAGER_IMPL_H_
