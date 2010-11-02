// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef CEEE_IE_BROKER_BROKER_DOCS_H_  // Mainly for lint
#define CEEE_IE_BROKER_BROKER_DOCS_H_

/** @page BrokerDoc Detailed documentation of the Broker process.

@section BrokerIntro Introduction

Since Internet Explorer can have multiple processes and run them at different
integrity level, we use a separate process to take care of integrity level
elevation when needed. This Broker process is also used as the sole
communication point with a Chrome Frame instance for Chrome Extension API
invocation to be executed in IE as well as events coming from IE and going back
to Chrome.

@subsection ApiInvocation API Invocation

The Broker must register a Chrome Frame Events sink to receive the private
messages used to redirect Chrome Extension API Invocations. But this has to be
done in a non-blocking single threaded way. So we must create a Single Thread
Apartment (STA) into which an instance of the ChromeFrameHost class will run, so
it can receive the private messages safely.

Once an API Invocation message is passed to the Broker via the Chrome Frame
Host, the request is simply added to a global queue of API Invocation, waiting
to be processed by a worker thread running in the Multi Thread Apartment (MTA).

Running the API invocation mostly likely need to execute code in the IE process,
but we can't block the STA for those calls, this is why we need to delegate
these calls to a work thread while the Chrome Frame Host STA can run free.

Once an API invocation is picked up from the queue, and processed (by making
calls to instances of the CeeeExecutor running in the IE process and managed
by the ExecutorsManager), then the API response can be sent back to Chrome, via
the Chrome Frame Host, in the exact same way that we would send Chrome Extension
Events back to Chrome as described below.

@subsection EventDispatching Event Dispatching

When an event occurs in IE, that Chrome Extension might be interested in, the
code running in IE fires an event on the ICeeeBroker interface and one of
the RPC threads, running in the MTA catch it. We process all that is needed
to fill the event data from the RPC thread, and then add the event to the
global queue of events that are to be dispatched to Chrome via the Chrome Frame
Host running in the STA.

So the Chrome Frame Host thread must wake up when events are added to the queue
and simply pass them to Chrome Frame before going back to sleep. Waiting for
further API invocations, or more events.

Since the API invocation responses need to go through the Chrome Frame Host
in the exact same way as the events (except for a different target string), we
can use the exact same mechanism to send back API invocation response as we do
for events.

**/

#endif  // CEEE_IE_BROKER_BROKER_DOCS_H_
