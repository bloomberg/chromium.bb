// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A class that manages a connection to an XMPP server.

#ifndef JINGLE_NOTIFIER_BASE_XMPP_CONNECTION_H_
#define JINGLE_NOTIFIER_BASE_XMPP_CONNECTION_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "net/url_request/url_request_context_getter.h"
#include "third_party/libjingle_xmpp/xmpp/xmppengine.h"
#include "webrtc/rtc_base/sigslot.h"

namespace buzz {
class PreXmppAuth;
class XmlElement;
class XmppClientSettings;
class XmppTaskParentInterface;
}  // namespace buzz

namespace jingle_glue {
class TaskPump;
}  // namespace jingle_glue

namespace notifier {

class WeakXmppClient;

class XmppConnection : public sigslot::has_slots<> {
 public:
  class Delegate {
   public:
    // Called (at most once) when a connection has been established.
    // |base_task| can be used by the client as the parent of any Task
    // it creates as long as it is valid (i.e., non-NULL).
    virtual void OnConnect(
        base::WeakPtr<buzz::XmppTaskParentInterface> base_task) = 0;

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
    // Ideally, |error| would always be set to something that is not
    // ERROR_NONE, but due to inconsistent error-handling this doesn't
    // always happen.
    virtual void OnError(buzz::XmppEngine::Error error, int subcode,
                         const buzz::XmlElement* stream_error) = 0;

   protected:
    virtual ~Delegate();
  };

  // Does not take ownership of |delegate|, which must not be
  // NULL.  Takes ownership of |pre_xmpp_auth|, which may be NULL.
  //
  // TODO(akalin): Avoid the need for |pre_xmpp_auth|.
  XmppConnection(const buzz::XmppClientSettings& xmpp_client_settings,
                 const scoped_refptr<net::URLRequestContextGetter>&
                     request_context_getter,
                 Delegate* delegate,
                 buzz::PreXmppAuth* pre_xmpp_auth);

  // Invalidates any weak pointers passed to the delegate by
  // OnConnect(), but does not trigger a call to the delegate's
  // OnError() function.
  ~XmppConnection() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(XmppConnectionTest, RaisedError);
  FRIEND_TEST_ALL_PREFIXES(XmppConnectionTest, Connect);
  FRIEND_TEST_ALL_PREFIXES(XmppConnectionTest, MultipleConnect);
  FRIEND_TEST_ALL_PREFIXES(XmppConnectionTest, ConnectThenError);
  FRIEND_TEST_ALL_PREFIXES(XmppConnectionTest,
                           TasksDontRunAfterXmppConnectionDestructor);

  void OnStateChange(buzz::XmppEngine::State state);
  void OnInputLog(const char* data, int len);
  void OnOutputLog(const char* data, int len);

  void ClearClient();

  std::unique_ptr<jingle_glue::TaskPump> task_pump_;
  base::WeakPtr<WeakXmppClient> weak_xmpp_client_;
  bool on_connect_called_;
  Delegate* delegate_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(XmppConnection);
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_BASE_XMPP_CONNECTION_H_
