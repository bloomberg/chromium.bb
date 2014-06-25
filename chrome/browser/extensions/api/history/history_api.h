// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_HISTORY_HISTORY_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_HISTORY_HISTORY_API_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/common/extensions/api/history.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"

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
                     const std::string& event_name,
                     scoped_ptr<base::ListValue> event_args);

  // Used for tracking registrations to history service notifications.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(HistoryEventRouter);
};

class HistoryAPI : public BrowserContextKeyedAPI, public EventRouter::Observer {
 public:
  explicit HistoryAPI(content::BrowserContext* context);
  virtual ~HistoryAPI();

  // KeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<HistoryAPI>* GetFactoryInstance();

  // EventRouter::Observer implementation.
  virtual void OnListenerAdded(const EventListenerInfo& details) OVERRIDE;

 private:
  friend class BrowserContextKeyedAPIFactory<HistoryAPI>;

  content::BrowserContext* browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "HistoryAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;

  // Created lazily upon OnListenerAdded.
  scoped_ptr<HistoryEventRouter> history_event_router_;
};

template <>
void BrowserContextKeyedAPIFactory<HistoryAPI>::DeclareFactoryDependencies();

// Base class for history function APIs.
class HistoryFunction : public ChromeAsyncExtensionFunction {
 protected:
  virtual ~HistoryFunction() {}

  bool ValidateUrl(const std::string& url_string, GURL* url);
  bool VerifyDeleteAllowed();
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
  virtual bool RunAsync() OVERRIDE;

  // Return true if the async call was completed, false otherwise.
  virtual bool RunAsyncImpl() = 0;

  // Call this method to report the results of the async method to the caller.
  // This method calls Release().
  virtual void SendAsyncResponse();

  // The consumer for the HistoryService callbacks.
  CancelableRequestConsumer cancelable_consumer_;
  base::CancelableTaskTracker task_tracker_;

 private:
  // The actual call to SendResponse.  This is required since the semantics for
  // CancelableRequestConsumerT require it to be accessed after the call.
  void SendResponseToCallback();
};

class HistoryGetVisitsFunction : public HistoryFunctionWithCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("history.getVisits", HISTORY_GETVISITS)

 protected:
  virtual ~HistoryGetVisitsFunction() {}

  // HistoryFunctionWithCallback:
  virtual bool RunAsyncImpl() OVERRIDE;

  // Callback for the history function to provide results.
  void QueryComplete(bool success,
                     const history::URLRow& url_row,
                     const history::VisitVector& visits);
};

class HistorySearchFunction : public HistoryFunctionWithCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("history.search", HISTORY_SEARCH)

 protected:
  virtual ~HistorySearchFunction() {}

  // HistoryFunctionWithCallback:
  virtual bool RunAsyncImpl() OVERRIDE;

  // Callback for the history function to provide results.
  void SearchComplete(history::QueryResults* results);
};

class HistoryAddUrlFunction : public HistoryFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("history.addUrl", HISTORY_ADDURL)

 protected:
  virtual ~HistoryAddUrlFunction() {}

  // HistoryFunctionWithCallback:
  virtual bool RunAsync() OVERRIDE;
};

class HistoryDeleteAllFunction : public HistoryFunctionWithCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("history.deleteAll", HISTORY_DELETEALL)

 protected:
  virtual ~HistoryDeleteAllFunction() {}

  // HistoryFunctionWithCallback:
  virtual bool RunAsyncImpl() OVERRIDE;

  // Callback for the history service to acknowledge deletion.
  void DeleteComplete();
};


class HistoryDeleteUrlFunction : public HistoryFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("history.deleteUrl", HISTORY_DELETEURL)

 protected:
  virtual ~HistoryDeleteUrlFunction() {}

  // HistoryFunctionWithCallback:
  virtual bool RunAsync() OVERRIDE;
};

class HistoryDeleteRangeFunction : public HistoryFunctionWithCallback {
 public:
  DECLARE_EXTENSION_FUNCTION("history.deleteRange", HISTORY_DELETERANGE)

 protected:
  virtual ~HistoryDeleteRangeFunction() {}

  // HistoryFunctionWithCallback:
  virtual bool RunAsyncImpl() OVERRIDE;

  // Callback for the history service to acknowledge deletion.
  void DeleteComplete();
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_HISTORY_HISTORY_API_H_
