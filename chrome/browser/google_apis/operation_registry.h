// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_OPERATION_REGISTRY_H_
#define CHROME_BROWSER_GOOGLE_APIS_OPERATION_REGISTRY_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/id_map.h"
#include "base/observer_list.h"
#include "base/time.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"

namespace google_apis {

class OperationRegistryObserver;

// Unique ID to identify each operation.
typedef int32 OperationID;

// Enumeration type for indicating the direction of the operation.
enum OperationType {
  OPERATION_UPLOAD,
  OPERATION_DOWNLOAD,
  OPERATION_OTHER,
};

// Enumeration type for indicating the state of the transfer.
enum OperationTransferState {
  OPERATION_NOT_STARTED,
  OPERATION_STARTED,
  OPERATION_IN_PROGRESS,
  OPERATION_COMPLETED,
  OPERATION_FAILED,
  OPERATION_SUSPENDED,
};

// Returns string representations of the operation type and state, which are
// exposed to the private extension API as in:
// operation.chrome/common/extensions/api/file_browser_private.json
std::string OperationTypeToString(OperationType type);
std::string OperationTransferStateToString(OperationTransferState state);

// Structure that packs progress information of each operation.
struct OperationProgressStatus {
  OperationProgressStatus(OperationType type, const base::FilePath& file_path);
  // For debugging
  std::string DebugString() const;

  OperationID operation_id;

  // Type of the operation: upload/download.
  OperationType operation_type;
  // GData path of the file dealt with the current operation.
  base::FilePath file_path;
  // Current state of the transfer;
  OperationTransferState transfer_state;
  // The time when the operation is initiated.
  base::Time start_time;
  // Current fraction of progress of the operation.
  int64 progress_current;
  // Expected total number of bytes to be transferred in the operation.
  // -1 if no expectation is available (yet).
  int64 progress_total;
};
typedef std::vector<OperationProgressStatus> OperationProgressStatusList;

// This class tracks all the in-flight GData operation objects and manage their
// lifetime.
class OperationRegistry {
 public:
  OperationRegistry();
  ~OperationRegistry();

  // Base class for operations that this registry class can maintain.
  // NotifyStart() passes the ownership of the Operation object to the registry.
  // In particular, calling NotifyFinish() causes the registry to delete the
  // Operation object itself.
  class Operation {
   public:
    explicit Operation(OperationRegistry* registry);
    Operation(OperationRegistry* registry,
              OperationType type,
              const base::FilePath& file_path);
    virtual ~Operation();

    // Cancels the ongoing operation. NotifyFinish() is called and the Operation
    // object is deleted once the cancellation is done in DoCancel().
    void Cancel();

    // Retrieves the current progress status of the operation.
    const OperationProgressStatus& progress_status() const {
      return progress_status_;
    }

   protected:
    // Notifies the registry about current status.
    void NotifyStart();
    void NotifyProgress(int64 current, int64 total);
    void NotifyFinish(OperationTransferState status);
    // Notifies suspend/resume, used for chunked upload operations.
    // The initial upload operation should issue "start" "progress"* "suspend".
    // The subsequent operations will call "resume" "progress"* "suspend",
    // and the last one will do "resume" "progress"* "finish".
    // In other words, "suspend" is similar to "finish" except it lasts to live
    // until the next "resume" comes. "Resume" is similar to "start", except
    // that it removes the existing "suspend" operation.
    void NotifySuspend();
    void NotifyResume();

   private:
    // Does the cancellation.
    virtual void DoCancel() = 0;

    OperationRegistry* const registry_;
    OperationProgressStatus progress_status_;
  };

  // Cancels all in-flight operations.
  void CancelAll();

  // Cancels ongoing operation for a given virtual |file_path|. Returns true if
  // the operation was found and canceled.
  bool CancelForFilePath(const base::FilePath& file_path);

  // Obtains the list of currently active operations.
  OperationProgressStatusList GetProgressStatusList();

  // Sets an observer. The registry do NOT own observers; before destruction
  // they need to be removed from the registry.
  void AddObserver(OperationRegistryObserver* observer);
  void RemoveObserver(OperationRegistryObserver* observer);

  // Disables the notification suppression for testing purpose.
  void DisableNotificationFrequencyControlForTest();

 private:
  // Handlers for notifications from Operations.
  friend class Operation;
  // Notifies that an operation has started. This method passes the ownership of
  // the operation to the registry. A fresh operation ID is returned to *id.
  void OnOperationStart(Operation* operation, OperationID* id);
  void OnOperationProgress(OperationID operation);
  void OnOperationFinish(OperationID operation);
  void OnOperationSuspend(OperationID operation);
  void OnOperationResume(Operation* operation,
                         OperationProgressStatus* new_status);

  bool IsFileTransferOperation(const Operation* operation) const;

  // Controls the frequency of notifications, not to flood the listeners with
  // too many events.
  bool ShouldNotifyStatusNow(const OperationProgressStatusList& list);
  // Sends notifications to the observers after checking that the frequency is
  // not too high by ShouldNotifyStatusNow.
  void NotifyStatusToObservers();

  // Cancels the specified operation.
  void CancelOperation(Operation* operation);

  typedef IDMap<Operation, IDMapOwnPointer> OperationIDMap;
  OperationIDMap in_flight_operations_;
  ObserverList<OperationRegistryObserver> observer_list_;
  base::Time last_notification_;
  bool do_notification_frequency_control_;

  DISALLOW_COPY_AND_ASSIGN(OperationRegistry);
};

// Observer interface for listening changes in the active set of operations.
class OperationRegistryObserver {
 public:
  // Called when a GData operation started, made some progress, or finished.
  virtual void OnProgressUpdate(const OperationProgressStatusList& list) {}

 protected:
  virtual ~OperationRegistryObserver() {}
};

}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_OPERATION_REGISTRY_H_
