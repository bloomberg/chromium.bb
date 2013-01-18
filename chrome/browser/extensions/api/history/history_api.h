// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_HISTORY_HISTORY_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_HISTORY_HISTORY_API_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/common/cancelable_task_tracker.h"
#include "chrome/common/extensions/api/history.h"
#include "content/public/browser/notification_registrar.h"

namespace base {
class ListValue;
}

namespace extensions {

// Observes History service and routes the notifications as events to the
// extension system.
class HistoryEventRouter : public content::NotificationObserver {
 public:
  explicit HistoryEventRouter(Profile* profile);
  virtual ~HistoryEventRouter();

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
                     scoped_ptr<base::ListValue> event_args);

  // Used for tracking registrations to history service notifications.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(HistoryEventRouter);
};

class HistoryAPI : public ProfileKeyedAPI,
                   public EventRouter::Observer {
 public:
  explicit HistoryAPI(Profile* profile);
  virtual ~HistoryAPI();

  // ProfileKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<HistoryAPI>* GetFactoryInstance();

  // EventRouter::Observer implementation.
  virtual void OnListenerAdded(const EventListenerInfo& details) OVERRIDE;

 private:
  friend class ProfileKeyedAPIFactory<HistoryAPI>;

  Profile* profile_;

  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "HistoryAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;

  // Created lazily upon OnListenerAdded.
  scoped_ptr<HistoryEventRouter> history_event_router_;
};

// Base class for history function APIs.
class HistoryFunction : public AsyncExtensionFunction {
 protected:
  virtual ~HistoryFunction() {}
  virtual void Run() OVERRIDE;

  bool ValidateUrl(const std::string& url_string, GURL* url);
  base::Time GetTime(double ms_from_epoch);
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
  CancelableTaskTracker task_tracker_;

 private:
  // The actual call to SendResponse.  This is required since the semantics for
  // CancelableRequestConsumerT require it to be accessed after the call.
  void SendResponseToCallback();
};

class HistoryGetMostVisitedFunction : public HistoryFunctionWithCallback {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.history.getMostVisited");

 protected:
  virtual ~HistoryGetMostVisitedFunction() {}

  // HistoryFunctionWithCallback:
  virtual bool RunAsyncImpl() OVERRIDE;

  // Callback for the history function to provide results.
  void QueryComplete(CancelableRequestProvider::Handle handle,
                     const history::FilteredURLList& data);
};

class HistoryGetVisitsFunction : public HistoryFunctionWithCallback {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("history.getVisits");

 protected:
  virtual ~HistoryGetVisitsFunction() {}

  // HistoryFunctionWithCallback:
  virtual bool RunAsyncImpl() OVERRIDE;

  // Callback for the history function to provide results.
  void QueryComplete(HistoryService::Handle request_service,
                     bool success,
                     const history::URLRow* url_row,
                     history::VisitVector* visits);
};

class HistorySearchFunction : public HistoryFunctionWithCallback {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("history.search");

 protected:
  virtual ~HistorySearchFunction() {}

  // HistoryFunctionWithCallback:
  virtual bool RunAsyncImpl() OVERRIDE;

  // Callback for the history function to provide results.
  void SearchComplete(HistoryService::Handle request_handle,
                      history::QueryResults* results);
};

class HistoryAddUrlFunction : public HistoryFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("history.addUrl");

 protected:
  virtual ~HistoryAddUrlFunction() {}

  // HistoryFunctionWithCallback:
  virtual bool RunImpl() OVERRIDE;
};

class HistoryDeleteAllFunction : public HistoryFunctionWithCallback {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("history.deleteAll");

 protected:
  virtual ~HistoryDeleteAllFunction() {}

  // HistoryFunctionWithCallback:
  virtual bool RunAsyncImpl() OVERRIDE;

  // Callback for the history service to acknowledge deletion.
  void DeleteComplete();
};


class HistoryDeleteUrlFunction : public HistoryFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("history.deleteUrl");

 protected:
  virtual ~HistoryDeleteUrlFunction() {}

  // HistoryFunctionWithCallback:
  virtual bool RunImpl() OVERRIDE;
};

class HistoryDeleteRangeFunction : public HistoryFunctionWithCallback {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("history.deleteRange");

 protected:
  virtual ~HistoryDeleteRangeFunction() {}

  // HistoryFunctionWithCallback:
  virtual bool RunAsyncImpl() OVERRIDE;

  // Callback for the history service to acknowledge deletion.
  void DeleteComplete();
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_HISTORY_HISTORY_API_H_
