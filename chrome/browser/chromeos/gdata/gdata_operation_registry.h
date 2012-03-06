// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_OPERATION_REGISTRY_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_OPERATION_REGISTRY_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
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

  // Structure that packs progress information of each operation.
  struct ProgressStatus {
    ProgressStatus();
    // For debugging
    std::string ToString() const;

    OperationID operation_id;

    // TODO(kinaba): http://crosbug.com/27371
    // Add more information for explaining what the operation is doing.
    //  (1) type of the operation: update/download.
    //  (2) virtual path of the file dealt with the operation.

    // The time when the operation is initiated.
    base::Time start_time;
    // Current fraction of progress of the operation.
    int64 progress_current;
    // Expected total number of bytes to be transferred in the operation.
    // -1 if no expectation is available (yet).
    int64 progress_total;
  };

  // Observer interface for listening changes in the active set of operations.
  class Observer {
   public:
    // Called when a GData operation started, made some progress, or finished.
    virtual void OnProgressUpdate(const std::vector<ProgressStatus>& list) = 0;
   protected:
    virtual ~Observer() {}
  };

  // Base class for operations that this registry class can maintain.
  // The Operation objects are owned by the registry. In particular, calling
  // NotifyFinish() causes the registry to delete the Operation object itself.
  class Operation {
   public:
    explicit Operation(GDataOperationRegistry* registry);
    virtual ~Operation();

    // Cancels the ongoing operation. NotifyFinish() is called and the Operation
    // object is deleted once the cancellation is done in DoCancel().
    void Cancel();

    // Retrieves the current progress status of the operation.
    const ProgressStatus& progress_status() const { return progress_status_; }

    // Enumeration type for indicating how an operation has ended.
    enum FinishStatus {
      OPERATION_FAILURE,
      OPERATION_SUCCESS
    };

   protected:
    // Notifies the registry about current status.
    void NotifyStart();
    void NotifyProgress(int64 current, int64 total);
    void NotifyFinish(FinishStatus status);

   private:
    // Does the cancellation.
    virtual void DoCancel() = 0;

    GDataOperationRegistry* const registry_;
    ProgressStatus progress_status_;

    // Used for internal assertions.
    bool is_started_;
    bool is_finished_;
  };

  // Cancels all in-flight operations.
  void CancelAll();

  // Obtains the list of currently active operations.
  std::vector<ProgressStatus> GetProgressStatusList();

  // Sets the observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // Handlers for notifications from Operations.
  friend class Operation;
  // The registry assigns a fresh operation ID and return it to *id.
  void OnOperationStart(Operation* operation, OperationID* id);
  void OnOperationProgress(OperationID operation);
  void OnOperationFinish(OperationID operation, Operation::FinishStatus status);

  typedef IDMap<Operation, IDMapOwnPointer> OperationIDMap;
  OperationIDMap in_flight_operations_;
  ObserverList<Observer> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(GDataOperationRegistry);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_OPERATION_REGISTRY_H_
