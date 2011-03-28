// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_HISTORY_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_HISTORY_API_H_
#pragma once

#include <map>
#include <string>

#include "base/memory/singleton.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_notifications.h"
#include "content/common/notification_registrar.h"

// Observes History service and routes the notifications as events to the
// extension system.
class ExtensionHistoryEventRouter : public NotificationObserver {
 public:
  // Single instance of the event router.
  static ExtensionHistoryEventRouter* GetInstance();

  // Safe to call multiple times.
  void ObserveProfile(Profile* profile);

 private:
  friend struct DefaultSingletonTraits<ExtensionHistoryEventRouter>;

  ExtensionHistoryEventRouter();
  virtual ~ExtensionHistoryEventRouter();

  // NotificationObserver::Observe.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  void HistoryUrlVisited(Profile* profile,
                         const history::URLVisitedDetails* details);

  void HistoryUrlsRemoved(Profile* profile,
                          const history::URLsDeletedDetails* details);

  void DispatchEvent(Profile* profile,
                     const char* event_name,
                     const std::string& json_args);

  // Used for tracking registrations to history service notifications.
  NotificationRegistrar registrar_;

  // Registered profiles.
  typedef std::map<uintptr_t, Profile*> ProfileMap;
  ProfileMap profiles_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionHistoryEventRouter);
};


// Base class for history function APIs.
class HistoryFunction : public AsyncExtensionFunction {
 public:
  virtual void Run();
  virtual bool RunImpl() = 0;

  bool GetUrlFromValue(Value* value, GURL* url);
  bool GetTimeFromValue(Value* value, base::Time* time);
};

// Base class for history funciton APIs which require async interaction with
// chrome services and the extension thread.
class HistoryFunctionWithCallback : public HistoryFunction {
 public:
  HistoryFunctionWithCallback();
  ~HistoryFunctionWithCallback();

  // Return true if the async call was completed, false otherwise.
  virtual bool RunAsyncImpl() = 0;

  // Call this method to report the results of the async method to the caller.
  // This method calls Release().
  virtual void SendAsyncResponse();

  // Override HistoryFunction::RunImpl.
  virtual bool RunImpl();

 protected:
  // The consumer for the HistoryService callbacks.
  CancelableRequestConsumer cancelable_consumer_;

 private:
  // The actual call to SendResponse.  This is required since the semantics for
  // CancelableRequestConsumerT require it to be accessed after the call.
  void SendResponseToCallback();
};

class GetVisitsHistoryFunction : public HistoryFunctionWithCallback {
 public:
  // Override HistoryFunction.
  virtual bool RunAsyncImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("history.getVisits");

  // Callback for the history function to provide results.
  void QueryComplete(HistoryService::Handle request_service,
                     bool success,
                     const history::URLRow* url_row,
                     history::VisitVector* visits);
};

class SearchHistoryFunction : public HistoryFunctionWithCallback {
 public:
  virtual bool RunAsyncImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("history.search");

  // Callback for the history function to provide results.
  void SearchComplete(HistoryService::Handle request_handle,
                      history::QueryResults* results);
};

class AddUrlHistoryFunction : public HistoryFunction {
 public:
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("history.addUrl");
};

class DeleteAllHistoryFunction : public HistoryFunctionWithCallback {
 public:
  virtual bool RunAsyncImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("history.deleteAll");

  // Callback for the history service to acknowledge deletion.
  void DeleteComplete();
};


class DeleteUrlHistoryFunction : public HistoryFunction {
 public:
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("history.deleteUrl");
};

class DeleteRangeHistoryFunction : public HistoryFunctionWithCallback {
 public:
  virtual bool RunAsyncImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("history.deleteRange");

  // Callback for the history service to acknowledge deletion.
  void DeleteComplete();
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_HISTORY_API_H_
