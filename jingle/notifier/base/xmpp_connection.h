// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A class that manages a connection to an XMPP server.

#ifndef JINGLE_NOTIFIER_BASE_XMPP_CONNECTION_H_
#define JINGLE_NOTIFIER_BASE_XMPP_CONNECTION_H_
#pragma once

#include "base/basictypes.h"
#include "base/non_thread_safe.h"
#include "base/scoped_ptr.h"
#include "base/weak_ptr.h"
#include "talk/base/sigslot.h"
#include "talk/xmpp/xmppengine.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace buzz {
class PreXmppAuth;
class XmlElement;
class XmppClientSettings;
}  // namespace

namespace talk_base {
class Task;
}  // namespace

namespace notifier {

class TaskPump;
class WeakXmppClient;

class XmppConnection : public sigslot::has_slots<> {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called (at most once) when a connection has been established.
    // |base_task| can be used by the client as the parent of any Task
    // it creates as long as it is valid (i.e., non-NULL).
    virtual void OnConnect(base::WeakPtr<talk_base::Task> base_task) = 0;

    // Called if an error has occurred (either before or after a call
    // to OnConnect()).  No calls to the delegate will be made after
    // this call.  Invalidates any weak pointers passed to the client
    // by OnConnect().
    //
    // |error| is the code for the raised error.  |subcode| is an
    // error-dependent subcode (0 if not applicable).  |stream_error|
    // is non-NULL iff |error| == ERROR_STREAM.  |stream_error| is
    // valid only for the lifetime of this function.
    //
    // Ideally, |error| would be set to something that is not
    // ERROR_NONE, but due to inconsistent error-handling this doesn't
    // always happen.
    virtual void OnError(buzz::XmppEngine::Error error, int subcode,
                         const buzz::XmlElement* stream_error) = 0;
  };

  // Does not take ownership of |delegate|, which may not be NULL.
  // Takes ownership of |pre_xmpp_auth|, which may be NULL.
  //
  // TODO(akalin): Avoid the need for |pre_xmpp_auth|.
  XmppConnection(const buzz::XmppClientSettings& xmpp_client_settings,
                 Delegate* delegate, buzz::PreXmppAuth* pre_xmpp_auth);

  // Invalidates any weak pointers passed to the delegate by
  // OnConnect(), but does not trigger a call to the delegate's
  // OnError() function.
  ~XmppConnection();

 private:
  void OnStateChange(buzz::XmppEngine::State state);
  void OnInputLog(const char* data, int len);
  void OnOutputLog(const char* data, int len);

  void ClearClient();

  NonThreadSafe non_thread_safe_;
  scoped_ptr<TaskPump> task_pump_;
  base::WeakPtr<WeakXmppClient> weak_xmpp_client_;
  bool on_connect_called_;
  Delegate* delegate_;

  FRIEND_TEST(XmppConnectionTest, RaisedError);
  FRIEND_TEST(XmppConnectionTest, Connect);
  FRIEND_TEST(XmppConnectionTest, MultipleConnect);
  FRIEND_TEST(XmppConnectionTest, ConnectThenError);

  DISALLOW_COPY_AND_ASSIGN(XmppConnection);
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_BASE_XMPP_CONNECTION_H_
