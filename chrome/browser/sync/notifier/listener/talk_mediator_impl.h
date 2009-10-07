// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This class is the interface between talk code and the client code proper
// It will manage all aspects of the connection and call back into the client
// when it needs attention (for instance if updates are available for syncing).

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_LISTENER_TALK_MEDIATOR_IMPL_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_LISTENER_TALK_MEDIATOR_IMPL_H_

#include <string>

#include "base/scoped_ptr.h"
#include "chrome/browser/sync/engine/auth_watcher.h"
#include "chrome/browser/sync/notifier/listener/mediator_thread.h"
#include "chrome/browser/sync/notifier/listener/talk_mediator.h"
#include "chrome/browser/sync/util/pthread_helpers.h"
#include "talk/xmpp/xmppclientsettings.h"
#include "testing/gtest/include/gtest/gtest_prod.h"  // For FRIEND_TEST

class EventListenerHookup;

namespace browser_sync {

class AuthWatcher;
struct AuthWatcherEvent;
class SyncerThread;

class TalkMediatorImpl
    : public TalkMediator,
      public sigslot::has_slots<> {
 public:
  TalkMediatorImpl();
  explicit TalkMediatorImpl(MediatorThread* thread);
  virtual ~TalkMediatorImpl();

  // Overriden from TalkMediator.
  virtual void WatchAuthWatcher(AuthWatcher* auth_watcher);
  virtual bool SetAuthToken(const std::string& email,
                            const std::string& token);
  virtual bool Login();
  virtual bool Logout();

  virtual bool SendNotification();

  TalkMediatorChannel* channel() const;

 private:
  struct TalkMediatorState {
    TalkMediatorState()
        : started(0), connected(0), initialized(0), logged_in(0),
          subscribed(0) {
    }

    unsigned int started : 1;      // Background thread has started.
    unsigned int connected : 1;    // Connected to the mediator thread signal.
    unsigned int initialized : 1;  // Initialized with login information.
    unsigned int logged_in : 1;    // Logged in the mediator's authenticator.
    unsigned int subscribed : 1;   // Subscribed to the xmpp receiving channel.
  };

  typedef PThreadScopedLock<PThreadMutex> MutexLock;

  // Completes common initialization between the constructors.  Set should
  // connect to true if the talk mediator should connect to the controlled
  // mediator thread's SignalStateChange object.
  void TalkMediatorInitialization(bool should_connect);

  // Called from the authwatcher after authentication completes.  Signals this
  // class to push listening and subscription events to the mediator thread.
  void AuthWatcherEventHandler(const AuthWatcherEvent& auth_event);

  // Callback for the mediator thread.
  void MediatorThreadMessageHandler(MediatorThread::MediatorMessage message);

  // Responses to messages from the MediatorThread.
  void OnNotificationReceived();
  void OnNotificationSent();
  void OnLogin();
  void OnLogout();
  void OnSubscriptionFailure();
  void OnSubscriptionSuccess();

  // Does the actual login funcationality, called from Login() and the
  // AuthWatcher event handler.
  bool DoLogin();

  // Mutex for synchronizing event access.  This class listens to two event
  // sources, Authwatcher and MediatorThread.  It can also be called by through
  // the TalkMediatorInteface.  All these access points are serialized by
  // this mutex.
  PThreadMutex mutex_;

  // Internal state.
  TalkMediatorState state_;

  // Cached and verfied from the SetAuthToken method.
  buzz::XmppClientSettings xmpp_settings_;

  // Interface to listen to authentication events.
  scoped_ptr<EventListenerHookup> auth_hookup_;

  // The worker thread through which talk events are posted and received.
  scoped_ptr<MediatorThread> mediator_thread_;

  // Channel through which to broadcast events.
  scoped_ptr<TalkMediatorChannel> channel_;

  FRIEND_TEST(TalkMediatorImplTest, SetAuthTokenWithBadInput);
  FRIEND_TEST(TalkMediatorImplTest, SetAuthTokenWithGoodInput);
  FRIEND_TEST(TalkMediatorImplTest, SendNotification);
  FRIEND_TEST(TalkMediatorImplTest, MediatorThreadCallbacks);
  DISALLOW_COPY_AND_ASSIGN(TalkMediatorImpl);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_LISTENER_TALK_MEDIATOR_IMPL_H_
