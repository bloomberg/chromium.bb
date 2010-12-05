// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// @file
// Broke Postman implementation.

#ifndef CEEE_IE_BROKER_CHROME_POSTMAN_H_
#define CEEE_IE_BROKER_CHROME_POSTMAN_H_

#include "base/singleton.h"
#include "base/thread.h"
#include "ceee/ie/common/chrome_frame_host.h"

#include "broker_lib.h"  // NOLINT

// The Broker Postman singleton object wrapping the ChromeFrameHost
// class so that it can receive API invocation from Chrome Frame, and also
// dispatch response and events to it.
//
// Since Chrome Frame must be access from a Single Thread Apartment, this
// object must only call it in an STA, so the PostMessage method must post a
// task to do so in the STA thread.
//
// But when Chrome Frame calls into this object, we can't block and thus
// the API invocations must be queued so that the API Dispatcher can
// fetch them from the queue, handle them appropriately in the MTA and
// post back the asynchronous response to Chrome Frame via our
// PostMessage method.

class ChromePostman
    : public CComObjectRootEx<CComMultiThreadModel>,
      public IChromeFrameHostEvents {
 public:
  BEGIN_COM_MAP(ChromePostman)
  END_COM_MAP()

  ChromePostman();
  virtual ~ChromePostman();

  // This posts a tasks to our STA thread so that it posts the given message
  // to Chrome Frame.
  virtual void PostMessage(BSTR message, BSTR target);

  // This posts a tasks to the Api Invocation thread to fire the given event.
  virtual void FireEvent(const char* event_name, const char* event_args);

  // This queues a generic tasks to be executed in the Api Invocation thread.
  virtual void QueueApiInvocationThreadTask(Task* task);

  // This creates both an STA and an MTA threads. We must make sure we only call
  // Chrome Frame from this STA. And since we can't block Chrome Frame we use
  // the MTA thread to executes API Invocation we receive from Chrome Frame.
  // We also create and initialize Chrome Framer from here.
  virtual void Init();

  // To cleanly terminate the threads, and our hooks into Chrome Frame.
  virtual void Term();

  // Returns our single instance held by the module.
  static ChromePostman* GetInstance() { return single_instance_; }

  class ChromePostmanThread : public base::Thread {
   public:
    ChromePostmanThread();

    // Called just prior to starting the message loop
    virtual void Init();

    // Called just after the message loop ends
    virtual void CleanUp();

    // Posts the message to our instance of Chrome Frame.
    // THIS CAN ONLY BE CALLED FROM OUR THREAD, and we DCHECK it.
    void PostMessage(BSTR message, BSTR target);

    // Retrieves the Chrome Frame host; should only be called from the
    // postman thread.
    void GetHost(IChromeFrameHost** host);

    // Resets Chrome Frame by tearing it down and restarting it.
    // We use this when we receive a channel error meaning Chrome has died.
    void ResetChromeFrame();

   protected:
    // Creates and initializes the chrome frame host.
    // CAN ONLY BE CALLED FROM THE STA!
    HRESULT InitializeChromeFrameHost();

    // Isolate the creation of the host so we can overload it to mock
    // the Chrome Frame Host in our tests.
    // CAN ONLY BE CALLED FROM THE STA!
    virtual HRESULT CreateChromeFrameHost();

    // Tears down the Chrome Frame host.
    void TeardownChromeFrameHost();

    // The Chrome Frame host handling a Chrome Frame instance for us.
    CComPtr<IChromeFrameHost> chrome_frame_host_;
  };

 protected:
  // Messages received from Chrome Frame are sent to the API dispatcher via
  // a task posted to our MTA thread.
  HRESULT OnCfReadyStateChanged(LONG state);
  HRESULT OnCfPrivateMessage(BSTR msg, BSTR origin, BSTR target);
  HRESULT OnCfExtensionReady(BSTR path, int response);
  HRESULT OnCfGetEnabledExtensionsComplete(SAFEARRAY* tab_delimited_paths);
  HRESULT OnCfGetExtensionApisToAutomate(BSTR* functions_enabled);
  HRESULT OnCfChannelError();

  class ApiInvocationWorkerThread : public base::Thread {
   public:
    ApiInvocationWorkerThread();

    // Called just prior to starting the message loop
    virtual void Init();

    // Called just after the message loop ends
    virtual void CleanUp();
  };

  // The STA in which we run.
  ChromePostmanThread chrome_postman_thread_;

  // The MTA thread to which we post API Invocations and Fired Events.
  ApiInvocationWorkerThread api_worker_thread_;

  // The number of times we tried to launch ChromeFrame.
  int frame_reset_count_;

  // "in the end, there should be only one!" :-)
  static ChromePostman* single_instance_;
};

#endif  // CEEE_IE_BROKER_CHROME_POSTMAN_H_
