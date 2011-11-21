// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MEDIA_MEDIA_INTERNALS_PROXY_H_
#define CHROME_BROWSER_UI_WEBUI_MEDIA_MEDIA_INTERNALS_PROXY_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "chrome/browser/media/media_internals_observer.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class IOThread;
class MediaInternalsMessageHandler;

namespace base {
class ListValue;
class Value;
}

// This class is a proxy between MediaInternals (on the IO thread) and
// MediaInternalsMessageHandler (on the UI thread).
// It is ref_counted to ensure that it completes all pending Tasks on both
// threads before destruction.
class MediaInternalsProxy
    : public MediaInternalsObserver,
      public base::RefCountedThreadSafe<
          MediaInternalsProxy,
          content::BrowserThread::DeleteOnUIThread>,
      public ChromeNetLog::ThreadSafeObserverImpl,
      public content::NotificationObserver {
 public:
  MediaInternalsProxy();

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Register a Handler and start receiving callbacks from MediaInternals.
  void Attach(MediaInternalsMessageHandler* handler);

  // Unregister the same and stop receiving callbacks.
  void Detach();

  // Have MediaInternals send all the data it has.
  void GetEverything();

  // MediaInternalsObserver implementation. Called on the IO thread.
  virtual void OnUpdate(const string16& update) OVERRIDE;

  // ChromeNetLog::ThreadSafeObserver implementation. Callable from any thread:
  virtual void OnAddEntry(net::NetLog::EventType type,
                          const base::TimeTicks& time,
                          const net::NetLog::Source& source,
                          net::NetLog::EventPhase phase,
                          net::NetLog::EventParameters* params) OVERRIDE;

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class DeleteTask<MediaInternalsProxy>;
  virtual ~MediaInternalsProxy();

  // Build a dictionary mapping constant names to values.
  base::Value* GetConstants();

  void ObserveMediaInternalsOnIOThread();
  void StopObservingMediaInternalsOnIOThread();
  void GetEverythingOnIOThread();
  void UpdateUIOnUIThread(const string16& update);

  // Put |entry| on a list of events to be sent to the page.
  void AddNetEventOnUIThread(base::Value* entry);

  // Send all pending events to the page.
  void SendNetEventsOnUIThread();

  // Call a JavaScript function on the page. Takes ownership of |args|.
  void CallJavaScriptFunctionOnUIThread(const std::string& function,
                                        base::Value* args);

  MediaInternalsMessageHandler* handler_;
  IOThread* io_thread_;
  scoped_ptr<base::ListValue> pending_net_updates_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(MediaInternalsProxy);
};

#endif  // CHROME_BROWSER_UI_WEBUI_MEDIA_MEDIA_INTERNALS_PROXY_H_
