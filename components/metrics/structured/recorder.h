// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_RECORDER_H_
#define COMPONENTS_METRICS_STRUCTURED_RECORDER_H_

#include <memory>

#include "base/callback_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequenced_task_runner.h"

namespace base {
class FilePath;
}

namespace metrics {
namespace structured {

class EventBase;

// Recorder is a singleton to help communicate with the
// StructuredMetricsProvider. It serves two purposes:
// 1. Begin the initialization of the StructuredMetricsProvider (see class
// comment for more details).
// 2. Add an event for the StructuredMetricsProvider to record.
//
// The StructuredMetricsProvider is owned by the MetricsService, but it needs to
// be accessible to any part of the codebase, via an EventBase subclass, to
// record events. The StructuredMetricsProvider registers itself as an observer
// of this singleton when recording is enabled, and calls to Record (for
// recording) or ProfileAdded (for initialization) are then forwarded to it.
class Recorder {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called on a call to Record.
    virtual void OnRecord(const EventBase& event) = 0;
    // Called on a call to ProfileAdded.
    virtual void OnProfileAdded(const base::FilePath& profile_path) = 0;
  };

  static Recorder* GetInstance();

  // Notifies observers of |event|,
  void Record(const EventBase& event);

  // Notifies the StructuredMetricsProvider that a profile has been added with
  // path |profile_path|. The first call to ProfileAdded initializes the
  // provider using the keys stored in |profile_path|, so care should be taken
  // to ensure the first call provides a |profile_path| suitable for metrics
  // collection.
  // TODO(crbug.com/1016655): When structured metrics expands beyond Chrome OS,
  // investigate whether initialization can be simplified for Chrome.
  void ProfileAdded(const base::FilePath& profile_path);

  void SetUiTaskRunner(
      const scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

 private:
  friend class base::NoDestructor<Recorder>;
  friend class StructuredMetricsProvider;

  Recorder();
  ~Recorder();
  Recorder(const Recorder&) = delete;
  Recorder& operator=(const Recorder&) = delete;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  base::ObserverList<Observer> observers_;
};

}  // namespace structured
}  // namespace metrics

#endif  // COMPONENTS_METRICS_STRUCTURED_RECORDER_H_
