// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_HISTORY_EXTENSION_API_H_
#define CHROME_BROWSER_HISTORY_HISTORY_EXTENSION_API_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_notifications.h"
#include "content/public/browser/notification_registrar.h"

// Observes History service and routes the notifications as events to the
// extension system.
class HistoryExtensionEventRouter : public content::NotificationObserver {
 public:
  explicit HistoryExtensionEventRouter();
  virtual ~HistoryExtensionEventRouter();

  void ObserveProfile(Profile* profile);

 private:
  // content::NotificationObserver::Observe.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void HistoryUrlVisited(Profile* profile,
                         const history::URLVisitedDetails* details);

  void HistoryUrlsRemoved(Profile* profile,
                          const history::URLsDeletedDetails* details);

  void DispatchEvent(Profile* profile,
                     const char* event_name,
                     const std::string& json_args);

  // Used for tracking registrations to history service notifications.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(HistoryExtensionEventRouter);
};


// Base class for history function APIs.
class HistoryFunction : public AsyncExtensionFunction {
 protected:
  virtual ~HistoryFunction() {}
  virtual void Run() OVERRIDE;

  bool GetUrlFromValue(base::Value* value, GURL* url);
  bool GetTimeFromValue(base::Value* value, base::Time* time);
};

// Base class for history funciton APIs which require async interaction with
// chrome services and the extension thread.
class HistoryFunctionWithCallback : public HistoryFunction {
 public:
  HistoryFunctionWithCallback();

 protected:
  virtual ~HistoryFunctionWithCallback();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

  // Return true if the async call was completed, false otherwise.
  virtual bool RunAsyncImpl() = 0;

  // Call this method to report the results of the async method to the caller.
  // This method calls Release().
  virtual void SendAsyncResponse();

  // The consumer for the HistoryService callbacks.
  CancelableRequestConsumer cancelable_consumer_;

 private:
  // The actual call to SendResponse.  This is required since the semantics for
  // CancelableRequestConsumerT require it to be accessed after the call.
  void SendResponseToCallback();
};

class GetVisitsHistoryFunction : public HistoryFunctionWithCallback {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("history.getVisits");

 protected:
  virtual ~GetVisitsHistoryFunction() {}

  // HistoryFunctionWithCallback:
  virtual bool RunAsyncImpl() OVERRIDE;

  // Callback for the history function to provide results.
  void QueryComplete(HistoryService::Handle request_service,
                     bool success,
                     const history::URLRow* url_row,
                     history::VisitVector* visits);
};

class SearchHistoryFunction : public HistoryFunctionWithCallback {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("history.search");

 protected:
  virtual ~SearchHistoryFunction() {}

  // HistoryFunctionWithCallback:
  virtual bool RunAsyncImpl() OVERRIDE;

  // Callback for the history function to provide results.
  void SearchComplete(HistoryService::Handle request_handle,
                      history::QueryResults* results);
};

class AddUrlHistoryFunction : public HistoryFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("history.addUrl");

 protected:
  virtual ~AddUrlHistoryFunction() {}

  // HistoryFunctionWithCallback:
  virtual bool RunImpl() OVERRIDE;
};

class DeleteAllHistoryFunction : public HistoryFunctionWithCallback {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("history.deleteAll");

 protected:
  virtual ~DeleteAllHistoryFunction() {}

  // HistoryFunctionWithCallback:
  virtual bool RunAsyncImpl() OVERRIDE;

  // Callback for the history service to acknowledge deletion.
  void DeleteComplete();
};


class DeleteUrlHistoryFunction : public HistoryFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("history.deleteUrl");

 protected:
  virtual ~DeleteUrlHistoryFunction() {}

  // HistoryFunctionWithCallback:
  virtual bool RunImpl() OVERRIDE;
};

class DeleteRangeHistoryFunction : public HistoryFunctionWithCallback {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("history.deleteRange");

 protected:
  virtual ~DeleteRangeHistoryFunction() {}

  // HistoryFunctionWithCallback:
  virtual bool RunAsyncImpl() OVERRIDE;

  // Callback for the history service to acknowledge deletion.
  void DeleteComplete();
};

#endif  // CHROME_BROWSER_HISTORY_HISTORY_EXTENSION_API_H_
