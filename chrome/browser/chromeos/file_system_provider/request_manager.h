// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_REQUEST_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_REQUEST_MANAGER_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/file_system_provider/notification_manager_interface.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/request_value.h"

namespace chromeos {
namespace file_system_provider {

// Request type, passed to RequestManager::CreateRequest. For logging purposes.
enum RequestType {
  REQUEST_UNMOUNT,
  GET_METADATA,
  READ_DIRECTORY,
  OPEN_FILE,
  CLOSE_FILE,
  READ_FILE,
  CREATE_DIRECTORY,
  DELETE_ENTRY,
  CREATE_FILE,
  COPY_ENTRY,
  MOVE_ENTRY,
  TRUNCATE,
  WRITE_FILE,
  TESTING
};

// Converts a request type to human-readable format.
std::string RequestTypeToString(RequestType type);

// Manages requests between the service, async utils and the providing
// extensions.
class RequestManager {
 public:
  // Handles requests. Each request implementation must implement
  // this interface.
  class HandlerInterface {
   public:
    virtual ~HandlerInterface() {}

    // Called when the request is created. Executes the request implementation.
    // Returns false in case of a execution failure.
    virtual bool Execute(int request_id) = 0;

    // Success callback invoked by the providing extension in response to
    // Execute(). It may be called more than once, until |has_more| is set to
    // false.
    virtual void OnSuccess(int request_id,
                           scoped_ptr<RequestValue> result,
                           bool has_more) = 0;

    // Error callback invoked by the providing extension in response to
    // Execute(). It can be called at most once. It can be also called if the
    // request is aborted due to a timeout.
    virtual void OnError(int request_id,
                         scoped_ptr<RequestValue> result,
                         base::File::Error error) = 0;
  };

  // Observes activities in the request manager.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the request is created.
    virtual void OnRequestCreated(int request_id, RequestType type) = 0;

    // Called when the request is destroyed.
    virtual void OnRequestDestroyed(int request_id) = 0;

    // Called when the request is executed.
    virtual void OnRequestExecuted(int request_id) = 0;

    // Called when the request is fulfilled with a success.
    virtual void OnRequestFulfilled(int request_id,
                                    const RequestValue& result,
                                    bool has_more) = 0;

    // Called when the request is rejected with an error.
    virtual void OnRequestRejected(int request_id,
                                   const RequestValue& result,
                                   base::File::Error error) = 0;

    // Called when the request is timeouted.
    virtual void OnRequestTimeouted(int request_id) = 0;
  };

  explicit RequestManager(NotificationManagerInterface* notification_manager);
  virtual ~RequestManager();

  // Creates a request and returns its request id (greater than 0). Returns 0 in
  // case of an error (eg. too many requests). The |type| argument indicates
  // what kind of request it is.
  int CreateRequest(RequestType type, scoped_ptr<HandlerInterface> handler);

  // Handles successful response for the |request_id|. If |has_more| is false,
  // then the request is disposed, after handling the |response|. On error,
  // returns false, and the request is disposed. |response| must not be NULL.
  bool FulfillRequest(int request_id,
                      scoped_ptr<RequestValue> response,
                      bool has_more);

  // Handles error response for the |request_id|. If handling the error fails,
  // returns false. Always disposes the request. |response| must not be NULL.
  bool RejectRequest(int request_id,
                     scoped_ptr<RequestValue> response,
                     base::File::Error error);

  // Sets a custom timeout for tests. The new timeout value will be applied to
  // new requests
  void SetTimeoutForTesting(const base::TimeDelta& timeout);

  // Gets number of active requests for logging purposes.
  // TODO(mtomasz): Introduce a logger class to gather more information
  size_t GetActiveRequestsForLogging() const;

  // Adds and removes observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  struct Request {
    Request();
    ~Request();

    // Timer for discarding the request during a timeout.
    base::OneShotTimer<RequestManager> timeout_timer;

    // Handler tied to this request.
    scoped_ptr<HandlerInterface> handler;

   private:
    DISALLOW_COPY_AND_ASSIGN(Request);
  };

  typedef std::map<int, Request*> RequestMap;

  // Destroys the request with the passed |request_id|.
  void DestroyRequest(int request_id);

  // Called when a request with |request_id| timeouts.
  void OnRequestTimeout(int request_id);

  // Called when an user either aborts the unresponsive request or lets it
  // continue.
  void OnUnresponsiveNotificationResult(
      int request_id,
      NotificationManagerInterface::NotificationResult result);

  // Resets the timeout timer for the specified request.
  void ResetTimer(int request_id);

  RequestMap requests_;
  NotificationManagerInterface* notification_manager_;  // Not owned.
  int next_id_;
  base::TimeDelta timeout_;
  base::WeakPtrFactory<RequestManager> weak_ptr_factory_;
  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(RequestManager);
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_REQUEST_MANAGER_H_
