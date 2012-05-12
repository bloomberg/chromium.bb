// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_OPERATION_REGISTRY_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_OPERATION_REGISTRY_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/id_map.h"
#include "base/observer_list.h"
#include "base/time.h"

namespace gdata {

// This class tracks all the in-flight GData operation objects and manage their
// lifetime.
class GDataOperationRegistry {
 public:
  GDataOperationRegistry();
  virtual ~GDataOperationRegistry();

  // Unique ID to identify each operation.
  typedef int32 OperationID;

  // Enumeration type for indicating the direction of the operation.
  enum OperationType {
    OPERATION_UPLOAD,
    OPERATION_DOWNLOAD,
    OPERATION_OTHER,
  };

  enum OperationTransferState {
    OPERATION_NOT_STARTED,
    OPERATION_STARTED,
    OPERATION_IN_PROGRESS,
    OPERATION_COMPLETED,
    OPERATION_FAILED,
    OPERATION_SUSPENDED,
  };

  static std::string OperationTypeToString(OperationType type);
  static std::string OperationTransferStateToString(
      OperationTransferState state);

  // Structure that packs progress information of each operation.
  struct ProgressStatus {
    ProgressStatus(OperationType type, const FilePath& file_path);
    // For debugging
    std::string ToString() const;

    OperationID operation_id;

    // Type of the operation: upload/download.
    OperationType operation_type;
    // GData path of the file dealt with the current operation.
    FilePath file_path;
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
  typedef std::vector<ProgressStatus> ProgressStatusList;

  // Observer interface for listening changes in the active set of operations.
  class Observer {
   public:
    // Called when a GData operation started, made some progress, or finished.
    virtual void OnProgressUpdate(const ProgressStatusList& list) = 0;
    // Called when GData authentication failed.
    virtual void OnAuthenticationFailed() {}
   protected:
    virtual ~Observer() {}
  };

  // Base class for operations that this registry class can maintain.
  // The Operation objects are owned by the registry. In particular, calling
  // NotifyFinish() causes the registry to delete the Operation object itself.
  class Operation {
   public:
    explicit Operation(GDataOperationRegistry* registry);
    Operation(GDataOperationRegistry* registry,
              OperationType type,
              const FilePath& file_path);
    virtual ~Operation();

    // Cancels the ongoing operation. NotifyFinish() is called and the Operation
    // object is deleted once the cancellation is done in DoCancel().
    void Cancel();

    // Retrieves the current progress status of the operation.
    const ProgressStatus& progress_status() const { return progress_status_; }

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
    // Notifies that authentication has failed.
    void NotifyAuthFailed();

   private:
    // Does the cancellation.
    virtual void DoCancel() = 0;

    GDataOperationRegistry* const registry_;
    ProgressStatus progress_status_;
  };

  // Cancels all in-flight operations.
  void CancelAll();

  // Cancels ongoing operation for a given virtual |file_path|. Returns true if
  // the operation was found and canceled.
  bool CancelForFilePath(const FilePath& file_path);

  // Obtains the list of currently active operations.
  ProgressStatusList GetProgressStatusList();

  // Sets the observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // Handlers for notifications from Operations.
  friend class Operation;
  // The registry assigns a fresh operation ID and return it to *id.
  void OnOperationStart(Operation* operation, OperationID* id);
  void OnOperationProgress(OperationID operation);
  void OnOperationFinish(OperationID operation);
  void OnOperationSuspend(OperationID operation);
  void OnOperationResume(Operation* operation, ProgressStatus* new_status);
  void OnOperationAuthFailed();

  bool IsFileTransferOperation(const Operation* operation) const;

  typedef IDMap<Operation, IDMapOwnPointer> OperationIDMap;
  OperationIDMap in_flight_operations_;
  ObserverList<Observer> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(GDataOperationRegistry);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_OPERATION_REGISTRY_H_
