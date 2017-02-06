// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CORE_COUNTERS_BROWSING_DATA_COUNTER_H_
#define COMPONENTS_BROWSING_DATA_CORE_COUNTERS_BROWSING_DATA_COUNTER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "components/prefs/pref_member.h"

class PrefService;

namespace browsing_data {

class BrowsingDataCounter {
 public:
  typedef int64_t ResultInt;

  // Base class of results returned by BrowsingDataCounter. When the computation
  // has started, an instance is returned to represent a pending result.
  class Result {
   public:
    explicit Result(const BrowsingDataCounter* source);
    virtual ~Result();

    const BrowsingDataCounter* source() const { return source_; }
    virtual bool Finished() const;

   private:
    const BrowsingDataCounter* source_;

    DISALLOW_COPY_AND_ASSIGN(Result);
  };

  // A subclass of Result returned when the computation has finished. The result
  // value can be retrieved by calling |Value()|. Some BrowsingDataCounter
  // subclasses might use a subclass of FinishedResult to provide more complex
  // results.
  class FinishedResult : public Result {
   public:
    FinishedResult(const BrowsingDataCounter* source, ResultInt value);
    ~FinishedResult() override;

    // Result:
    bool Finished() const override;

    ResultInt Value() const;

   private:
    ResultInt value_;

    DISALLOW_COPY_AND_ASSIGN(FinishedResult);
  };

  typedef base::Callback<void(std::unique_ptr<Result>)> Callback;

  // Every calculation progresses through a state machine. At initialization,
  // the counter is IDLE. If a result is calculated within a given time
  // interval, it is immediately reported and the counter is again IDLE.
  // Otherwise, the counter instructs the UI to show a "Calculating..."
  // message and transitions to the SHOW_CALCULATING state. The counter stays
  // in this state for a given amount of time. If a result is calculated at
  // this time, it is stored, but not immediately reported. After the timer
  // elapses, we check if a result has been reported in the meantime. If yes,
  // we transition to REPORT_STAGED_RESULT, report the result, and then return
  // to IDLE. If not, we transition to READY_TO_REPORT_RESULT. In this
  // state, we wait for the calculation to finish. When that happens, we show
  // the result, and  return to the IDLE state.
  enum class State {
    IDLE,
    RESTARTED,
    SHOW_CALCULATING,
    REPORT_STAGED_RESULT,
    READY_TO_REPORT_RESULT,
  };

  BrowsingDataCounter();
  virtual ~BrowsingDataCounter();

  // Should be called once to initialize this class.
  void Init(PrefService* pref_service, const Callback& callback);

  // Name of the preference associated with this counter.
  virtual const char* GetPrefName() const = 0;

  // PrefService that manages the preferences for the user profile
  // associated with this counter.
  PrefService* GetPrefs() const;

  // Restarts the counter. Will be called automatically if the counting needs
  // to be restarted, e.g. when the deletion preference changes state or when
  // we are notified of data changes.
  void Restart();

  // Returns the state transition of this counter since past restart.
  // Used only for testing.
  const std::vector<State>& GetStateTransitionsForTesting();

 protected:
  // Should be called from |Count| by any overriding class to indicate that
  // counting is finished and report |value| as the result.
  void ReportResult(ResultInt value);

  // A convenience overload of the previous method that allows subclasses to
  // provide a custom |result|.
  void ReportResult(std::unique_ptr<Result> result);

  // A synchronous implementation of ReportResult(). Called immediately in the
  // RESTARTED and READY_TO_REPORT_RESULT states, called later if the counter is
  // in the SHOW_CALCULATING stage. This method is made virtual to be overriden
  // in tests.
  virtual void DoReportResult(std::unique_ptr<Result> result);

  // Calculates the beginning of the counting period as |period_| before now.
  base::Time GetPeriodStart();

 private:
  // Called after the class is initialized by calling |Init|.
  virtual void OnInitialized();

  // Count the data.
  virtual void Count() = 0;

  // State transition methods.
  void TransitionToShowCalculating();
  void TransitionToReadyToReportResult();

  // Pointer to the PrefService that manages the preferences for the user
  // profile associated with this counter.
  PrefService* pref_service_;

  // The callback that will be called when the UI should be updated with a new
  // counter value.
  Callback callback_;

  // The boolean preference indicating whether this data type is to be deleted.
  // If false, we will not count it.
  BooleanPrefMember pref_;

  // The integer preference describing the time period for which this data type
  // is to be deleted.
  IntegerPrefMember period_;

  // Whether this class was properly initialized by calling |Init|.
  bool initialized_;

  // State of the counter.
  State state_;

  // State transitions since the last restart.
  std::vector<State> state_transitions_;

  // A result is staged if it arrives during the SHOW_CALCULATING state.
  std::unique_ptr<Result> staged_result_;

  // A timer to time the RESTARTED->SHOW_CALCULATION and
  // SHOW_CALCULATION->READY_TO_REPORT_RESULT state transitions.
  base::OneShotTimer timer_;
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CORE_COUNTERS_BROWSING_DATA_COUNTER_H_
