// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/tracked_objects.h"

#include <limits.h>
#include <stdlib.h>

#include "base/atomicops.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/leak_annotations.h"
#include "base/logging.h"
#include "base/process/process_handle.h"
#include "base/profiler/alternate_timer.h"
#include "base/strings/stringprintf.h"
#include "base/third_party/valgrind/memcheck.h"
#include "base/tracking_info.h"

using base::TimeDelta;

namespace base {
class TimeDelta;
}

namespace tracked_objects {

namespace {
// Flag to compile out almost all of the task tracking code.
const bool kTrackAllTaskObjects = true;

// TODO(jar): Evaluate the perf impact of enabling this.  If the perf impact is
// negligible, enable by default.
// Flag to compile out parent-child link recording.
const bool kTrackParentChildLinks = false;

// When ThreadData is first initialized, should we start in an ACTIVE state to
// record all of the startup-time tasks, or should we start up DEACTIVATED, so
// that we only record after parsing the command line flag --enable-tracking.
// Note that the flag may force either state, so this really controls only the
// period of time up until that flag is parsed. If there is no flag seen, then
// this state may prevail for much or all of the process lifetime.
const ThreadData::Status kInitialStartupState =
    ThreadData::PROFILING_CHILDREN_ACTIVE;

// Control whether an alternate time source (Now() function) is supported by
// the ThreadData class.  This compile time flag should be set to true if we
// want other modules (such as a memory allocator, or a thread-specific CPU time
// clock) to be able to provide a thread-specific Now() function.  Without this
// compile-time flag, the code will only support the wall-clock time.  This flag
// can be flipped to efficiently disable this path (if there is a performance
// problem with its presence).
static const bool kAllowAlternateTimeSourceHandling = true;

// Possible states of the profiler timing enabledness.
enum {
  UNDEFINED_TIMING,
  ENABLED_TIMING,
  DISABLED_TIMING,
};

// State of the profiler timing enabledness.
base::subtle::Atomic32 g_profiler_timing_enabled = UNDEFINED_TIMING;

// Returns whether profiler timing is enabled. The default is true, but this may
// be overridden by a command-line flag. Some platforms may programmatically set
// this command-line flag to the "off" value if it's not specified.
// This in turn can be overridden by explicitly calling
// ThreadData::EnableProfilerTiming, say, based on a field trial.
inline bool IsProfilerTimingEnabled() {
  // Reading |g_profiler_timing_enabled| is done without barrier because
  // multiple initialization is not an issue while the barrier can be relatively
  // costly given that this method is sometimes called in a tight loop.
  base::subtle::Atomic32 current_timing_enabled =
      base::subtle::NoBarrier_Load(&g_profiler_timing_enabled);
  if (current_timing_enabled == UNDEFINED_TIMING) {
    if (!base::CommandLine::InitializedForCurrentProcess())
      return true;
    current_timing_enabled =
        (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
             switches::kProfilerTiming) ==
         switches::kProfilerTimingDisabledValue)
            ? DISABLED_TIMING
            : ENABLED_TIMING;
    base::subtle::NoBarrier_Store(&g_profiler_timing_enabled,
                                  current_timing_enabled);
  }
  return current_timing_enabled == ENABLED_TIMING;
}

}  // namespace

//------------------------------------------------------------------------------
// DeathData tallies durations when a death takes place.

DeathData::DeathData() {
  Clear();
}

DeathData::DeathData(int count) {
  Clear();
  count_ = count;
}

// TODO(jar): I need to see if this macro to optimize branching is worth using.
//
// This macro has no branching, so it is surely fast, and is equivalent to:
//             if (assign_it)
//               target = source;
// We use a macro rather than a template to force this to inline.
// Related code for calculating max is discussed on the web.
#define CONDITIONAL_ASSIGN(assign_it, target, source) \
    ((target) ^= ((target) ^ (source)) & -static_cast<int32>(assign_it))

void DeathData::RecordDeath(const int32 queue_duration,
                            const int32 run_duration,
                            const uint32 random_number) {
  // We'll just clamp at INT_MAX, but we should note this in the UI as such.
  if (count_ < INT_MAX)
    ++count_;
  queue_duration_sum_ += queue_duration;
  run_duration_sum_ += run_duration;

  if (queue_duration_max_ < queue_duration)
    queue_duration_max_ = queue_duration;
  if (run_duration_max_ < run_duration)
    run_duration_max_ = run_duration;

  // Take a uniformly distributed sample over all durations ever supplied.
  // The probability that we (instead) use this new sample is 1/count_.  This
  // results in a completely uniform selection of the sample (at least when we
  // don't clamp count_... but that should be inconsequentially likely).
  // We ignore the fact that we correlated our selection of a sample to the run
  // and queue times (i.e., we used them to generate random_number).
  CHECK_GT(count_, 0);
  if (0 == (random_number % count_)) {
    queue_duration_sample_ = queue_duration;
    run_duration_sample_ = run_duration;
  }
}

int DeathData::count() const { return count_; }

int32 DeathData::run_duration_sum() const { return run_duration_sum_; }

int32 DeathData::run_duration_max() const { return run_duration_max_; }

int32 DeathData::run_duration_sample() const {
  return run_duration_sample_;
}

int32 DeathData::queue_duration_sum() const {
  return queue_duration_sum_;
}

int32 DeathData::queue_duration_max() const {
  return queue_duration_max_;
}

int32 DeathData::queue_duration_sample() const {
  return queue_duration_sample_;
}

void DeathData::Clear() {
  count_ = 0;
  run_duration_sum_ = 0;
  run_duration_max_ = 0;
  run_duration_sample_ = 0;
  queue_duration_sum_ = 0;
  queue_duration_max_ = 0;
  queue_duration_sample_ = 0;
}

//------------------------------------------------------------------------------
DeathDataSnapshot::DeathDataSnapshot()
    : count(-1),
      run_duration_sum(-1),
      run_duration_max(-1),
      run_duration_sample(-1),
      queue_duration_sum(-1),
      queue_duration_max(-1),
      queue_duration_sample(-1) {
}

DeathDataSnapshot::DeathDataSnapshot(
    const tracked_objects::DeathData& death_data)
    : count(death_data.count()),
      run_duration_sum(death_data.run_duration_sum()),
      run_duration_max(death_data.run_duration_max()),
      run_duration_sample(death_data.run_duration_sample()),
      queue_duration_sum(death_data.queue_duration_sum()),
      queue_duration_max(death_data.queue_duration_max()),
      queue_duration_sample(death_data.queue_duration_sample()) {
}

DeathDataSnapshot::~DeathDataSnapshot() {
}

//------------------------------------------------------------------------------
BirthOnThread::BirthOnThread(const Location& location,
                             const ThreadData& current)
    : location_(location),
      birth_thread_(&current) {
}

//------------------------------------------------------------------------------
BirthOnThreadSnapshot::BirthOnThreadSnapshot() {
}

BirthOnThreadSnapshot::BirthOnThreadSnapshot(
    const tracked_objects::BirthOnThread& birth)
    : location(birth.location()),
      thread_name(birth.birth_thread()->thread_name()) {
}

BirthOnThreadSnapshot::~BirthOnThreadSnapshot() {
}

//------------------------------------------------------------------------------
Births::Births(const Location& location, const ThreadData& current)
    : BirthOnThread(location, current),
      birth_count_(1) { }

int Births::birth_count() const { return birth_count_; }

void Births::RecordBirth() { ++birth_count_; }

//------------------------------------------------------------------------------
// ThreadData maintains the central data for all births and deaths on a single
// thread.

// TODO(jar): We should pull all these static vars together, into a struct, and
// optimize layout so that we benefit from locality of reference during accesses
// to them.

// static
NowFunction* ThreadData::now_function_ = NULL;

// static
bool ThreadData::now_function_is_time_ = false;

// A TLS slot which points to the ThreadData instance for the current thread. We
// do a fake initialization here (zeroing out data), and then the real in-place
// construction happens when we call tls_index_.Initialize().
// static
base::ThreadLocalStorage::StaticSlot ThreadData::tls_index_ = TLS_INITIALIZER;

// static
int ThreadData::worker_thread_data_creation_count_ = 0;

// static
int ThreadData::cleanup_count_ = 0;

// static
int ThreadData::incarnation_counter_ = 0;

// static
ThreadData* ThreadData::all_thread_data_list_head_ = NULL;

// static
ThreadData* ThreadData::first_retired_worker_ = NULL;

// static
base::LazyInstance<base::Lock>::Leaky
    ThreadData::list_lock_ = LAZY_INSTANCE_INITIALIZER;

// static
ThreadData::Status ThreadData::status_ = ThreadData::UNINITIALIZED;

ThreadData::ThreadData(const std::string& suggested_name)
    : next_(NULL),
      next_retired_worker_(NULL),
      worker_thread_number_(0),
      incarnation_count_for_pool_(-1),
      current_stopwatch_(NULL) {
  DCHECK_GE(suggested_name.size(), 0u);
  thread_name_ = suggested_name;
  PushToHeadOfList();  // Which sets real incarnation_count_for_pool_.
}

ThreadData::ThreadData(int thread_number)
    : next_(NULL),
      next_retired_worker_(NULL),
      worker_thread_number_(thread_number),
      incarnation_count_for_pool_(-1),
      current_stopwatch_(NULL) {
  CHECK_GT(thread_number, 0);
  base::StringAppendF(&thread_name_, "WorkerThread-%d", thread_number);
  PushToHeadOfList();  // Which sets real incarnation_count_for_pool_.
}

ThreadData::~ThreadData() {}

void ThreadData::PushToHeadOfList() {
  // Toss in a hint of randomness (atop the uniniitalized value).
  (void)VALGRIND_MAKE_MEM_DEFINED_IF_ADDRESSABLE(&random_number_,
                                                 sizeof(random_number_));
  MSAN_UNPOISON(&random_number_, sizeof(random_number_));
  random_number_ += static_cast<uint32>(this - static_cast<ThreadData*>(0));
  random_number_ ^= (Now() - TrackedTime()).InMilliseconds();

  DCHECK(!next_);
  base::AutoLock lock(*list_lock_.Pointer());
  incarnation_count_for_pool_ = incarnation_counter_;
  next_ = all_thread_data_list_head_;
  all_thread_data_list_head_ = this;
}

// static
ThreadData* ThreadData::first() {
  base::AutoLock lock(*list_lock_.Pointer());
  return all_thread_data_list_head_;
}

ThreadData* ThreadData::next() const { return next_; }

// static
void ThreadData::InitializeThreadContext(const std::string& suggested_name) {
  if (!Initialize())  // Always initialize if needed.
    return;
  ThreadData* current_thread_data =
      reinterpret_cast<ThreadData*>(tls_index_.Get());
  if (current_thread_data)
    return;  // Browser tests instigate this.
  current_thread_data = new ThreadData(suggested_name);
  tls_index_.Set(current_thread_data);
}

// static
ThreadData* ThreadData::Get() {
  if (!tls_index_.initialized())
    return NULL;  // For unittests only.
  ThreadData* registered = reinterpret_cast<ThreadData*>(tls_index_.Get());
  if (registered)
    return registered;

  // We must be a worker thread, since we didn't pre-register.
  ThreadData* worker_thread_data = NULL;
  int worker_thread_number = 0;
  {
    base::AutoLock lock(*list_lock_.Pointer());
    if (first_retired_worker_) {
      worker_thread_data = first_retired_worker_;
      first_retired_worker_ = first_retired_worker_->next_retired_worker_;
      worker_thread_data->next_retired_worker_ = NULL;
    } else {
      worker_thread_number = ++worker_thread_data_creation_count_;
    }
  }

  // If we can't find a previously used instance, then we have to create one.
  if (!worker_thread_data) {
    DCHECK_GT(worker_thread_number, 0);
    worker_thread_data = new ThreadData(worker_thread_number);
  }
  DCHECK_GT(worker_thread_data->worker_thread_number_, 0);

  tls_index_.Set(worker_thread_data);
  return worker_thread_data;
}

// static
void ThreadData::OnThreadTermination(void* thread_data) {
  DCHECK(thread_data);  // TLS should *never* call us with a NULL.
  // We must NOT do any allocations during this callback. There is a chance
  // that the allocator is no longer active on this thread.
  if (!kTrackAllTaskObjects)
    return;  // Not compiled in.
  reinterpret_cast<ThreadData*>(thread_data)->OnThreadTerminationCleanup();
}

void ThreadData::OnThreadTerminationCleanup() {
  // The list_lock_ was created when we registered the callback, so it won't be
  // allocated here despite the lazy reference.
  base::AutoLock lock(*list_lock_.Pointer());
  if (incarnation_counter_ != incarnation_count_for_pool_)
    return;  // ThreadData was constructed in an earlier unit test.
  ++cleanup_count_;
  // Only worker threads need to be retired and reused.
  if (!worker_thread_number_) {
    return;
  }
  // We must NOT do any allocations during this callback.
  // Using the simple linked lists avoids all allocations.
  DCHECK_EQ(this->next_retired_worker_, reinterpret_cast<ThreadData*>(NULL));
  this->next_retired_worker_ = first_retired_worker_;
  first_retired_worker_ = this;
}

// static
void ThreadData::Snapshot(ProcessDataSnapshot* process_data_snapshot) {
  ThreadData::SnapshotCurrentPhase(
      &process_data_snapshot->phased_process_data_snapshots[0]);
}

Births* ThreadData::TallyABirth(const Location& location) {
  BirthMap::iterator it = birth_map_.find(location);
  Births* child;
  if (it != birth_map_.end()) {
    child =  it->second;
    child->RecordBirth();
  } else {
    child = new Births(location, *this);  // Leak this.
    // Lock since the map may get relocated now, and other threads sometimes
    // snapshot it (but they lock before copying it).
    base::AutoLock lock(map_lock_);
    birth_map_[location] = child;
  }

  if (kTrackParentChildLinks && status_ > PROFILING_ACTIVE &&
      !parent_stack_.empty()) {
    const Births* parent = parent_stack_.top();
    ParentChildPair pair(parent, child);
    if (parent_child_set_.find(pair) == parent_child_set_.end()) {
      // Lock since the map may get relocated now, and other threads sometimes
      // snapshot it (but they lock before copying it).
      base::AutoLock lock(map_lock_);
      parent_child_set_.insert(pair);
    }
  }

  return child;
}

void ThreadData::TallyADeath(const Births& birth,
                             int32 queue_duration,
                             const TaskStopwatch& stopwatch) {
  int32 run_duration = stopwatch.RunDurationMs();

  // Stir in some randomness, plus add constant in case durations are zero.
  const uint32 kSomePrimeNumber = 2147483647;
  random_number_ += queue_duration + run_duration + kSomePrimeNumber;
  // An address is going to have some randomness to it as well ;-).
  random_number_ ^= static_cast<uint32>(&birth - reinterpret_cast<Births*>(0));

  // We don't have queue durations without OS timer. OS timer is automatically
  // used for task-post-timing, so the use of an alternate timer implies all
  // queue times are invalid, unless it was explicitly said that we can trust
  // the alternate timer.
  if (kAllowAlternateTimeSourceHandling &&
      now_function_ &&
      !now_function_is_time_) {
    queue_duration = 0;
  }

  DeathMap::iterator it = death_map_.find(&birth);
  DeathData* death_data;
  if (it != death_map_.end()) {
    death_data = &it->second;
  } else {
    base::AutoLock lock(map_lock_);  // Lock as the map may get relocated now.
    death_data = &death_map_[&birth];
  }  // Release lock ASAP.
  death_data->RecordDeath(queue_duration, run_duration, random_number_);

  if (!kTrackParentChildLinks)
    return;
  if (!parent_stack_.empty()) {  // We might get turned off.
    DCHECK_EQ(parent_stack_.top(), &birth);
    parent_stack_.pop();
  }
}

// static
Births* ThreadData::TallyABirthIfActive(const Location& location) {
  if (!kTrackAllTaskObjects)
    return NULL;  // Not compiled in.

  if (!TrackingStatus())
    return NULL;
  ThreadData* current_thread_data = Get();
  if (!current_thread_data)
    return NULL;
  return current_thread_data->TallyABirth(location);
}

// static
void ThreadData::TallyRunOnNamedThreadIfTracking(
    const base::TrackingInfo& completed_task,
    const TaskStopwatch& stopwatch) {
  if (!kTrackAllTaskObjects)
    return;  // Not compiled in.

  // Even if we have been DEACTIVATED, we will process any pending births so
  // that our data structures (which counted the outstanding births) remain
  // consistent.
  const Births* birth = completed_task.birth_tally;
  if (!birth)
    return;
  ThreadData* current_thread_data = stopwatch.GetThreadData();
  if (!current_thread_data)
    return;

  // Watch out for a race where status_ is changing, and hence one or both
  // of start_of_run or end_of_run is zero.  In that case, we didn't bother to
  // get a time value since we "weren't tracking" and we were trying to be
  // efficient by not calling for a genuine time value. For simplicity, we'll
  // use a default zero duration when we can't calculate a true value.
  TrackedTime start_of_run = stopwatch.StartTime();
  int32 queue_duration = 0;
  if (!start_of_run.is_null()) {
    queue_duration = (start_of_run - completed_task.EffectiveTimePosted())
        .InMilliseconds();
  }
  current_thread_data->TallyADeath(*birth, queue_duration, stopwatch);
}

// static
void ThreadData::TallyRunOnWorkerThreadIfTracking(
    const Births* birth,
    const TrackedTime& time_posted,
    const TaskStopwatch& stopwatch) {
  if (!kTrackAllTaskObjects)
    return;  // Not compiled in.

  // Even if we have been DEACTIVATED, we will process any pending births so
  // that our data structures (which counted the outstanding births) remain
  // consistent.
  if (!birth)
    return;

  // TODO(jar): Support the option to coalesce all worker-thread activity under
  // one ThreadData instance that uses locks to protect *all* access.  This will
  // reduce memory (making it provably bounded), but run incrementally slower
  // (since we'll use locks on TallyABirth and TallyADeath).  The good news is
  // that the locks on TallyADeath will be *after* the worker thread has run,
  // and hence nothing will be waiting for the completion (... besides some
  // other thread that might like to run).  Also, the worker threads tasks are
  // generally longer, and hence the cost of the lock may perchance be amortized
  // over the long task's lifetime.
  ThreadData* current_thread_data = stopwatch.GetThreadData();
  if (!current_thread_data)
    return;

  TrackedTime start_of_run = stopwatch.StartTime();
  int32 queue_duration = 0;
  if (!start_of_run.is_null()) {
    queue_duration = (start_of_run - time_posted).InMilliseconds();
  }
  current_thread_data->TallyADeath(*birth, queue_duration, stopwatch);
}

// static
void ThreadData::TallyRunInAScopedRegionIfTracking(
    const Births* birth,
    const TaskStopwatch& stopwatch) {
  if (!kTrackAllTaskObjects)
    return;  // Not compiled in.

  // Even if we have been DEACTIVATED, we will process any pending births so
  // that our data structures (which counted the outstanding births) remain
  // consistent.
  if (!birth)
    return;

  ThreadData* current_thread_data = stopwatch.GetThreadData();
  if (!current_thread_data)
    return;

  int32 queue_duration = 0;
  current_thread_data->TallyADeath(*birth, queue_duration, stopwatch);
}

// static
void ThreadData::SnapshotAllExecutedTasks(
    ProcessDataPhaseSnapshot* process_data_phase,
    BirthCountMap* birth_counts) {
  if (!kTrackAllTaskObjects)
    return;  // Not compiled in.

  // Get an unchanging copy of a ThreadData list.
  ThreadData* my_list = ThreadData::first();

  // Gather data serially.
  // This hackish approach *can* get some slighly corrupt tallies, as we are
  // grabbing values without the protection of a lock, but it has the advantage
  // of working even with threads that don't have message loops.  If a user
  // sees any strangeness, they can always just run their stats gathering a
  // second time.
  for (ThreadData* thread_data = my_list;
       thread_data;
       thread_data = thread_data->next()) {
    thread_data->SnapshotExecutedTasks(process_data_phase, birth_counts);
  }
}

// static
void ThreadData::SnapshotCurrentPhase(
    ProcessDataPhaseSnapshot* process_data_phase) {
  // Add births that have run to completion to |collected_data|.
  // |birth_counts| tracks the total number of births recorded at each location
  // for which we have not seen a death count.
  BirthCountMap birth_counts;
  ThreadData::SnapshotAllExecutedTasks(process_data_phase, &birth_counts);

  // Add births that are still active -- i.e. objects that have tallied a birth,
  // but have not yet tallied a matching death, and hence must be either
  // running, queued up, or being held in limbo for future posting.
  for (const auto& birth_count : birth_counts) {
    if (birth_count.second > 0) {
      process_data_phase->tasks.push_back(TaskSnapshot(
          *birth_count.first, DeathData(birth_count.second), "Still_Alive"));
    }
  }
}

void ThreadData::SnapshotExecutedTasks(
    ProcessDataPhaseSnapshot* process_data_phase,
    BirthCountMap* birth_counts) {
  // Get copy of data, so that the data will not change during the iterations
  // and processing.
  ThreadData::BirthMap birth_map;
  ThreadData::DeathMap death_map;
  ThreadData::ParentChildSet parent_child_set;
  SnapshotMaps(&birth_map, &death_map, &parent_child_set);

  for (const auto& death : death_map) {
    process_data_phase->tasks.push_back(
        TaskSnapshot(*death.first, death.second, thread_name()));
    (*birth_counts)[death.first] -= death.first->birth_count();
  }

  for (const auto& birth : birth_map) {
    (*birth_counts)[birth.second] += birth.second->birth_count();
  }

  if (!kTrackParentChildLinks)
    return;

  for (const auto& parent_child : parent_child_set) {
    process_data_phase->descendants.push_back(
        ParentChildPairSnapshot(parent_child));
  }
}

// This may be called from another thread.
void ThreadData::SnapshotMaps(BirthMap* birth_map,
                              DeathMap* death_map,
                              ParentChildSet* parent_child_set) {
  base::AutoLock lock(map_lock_);
  for (const auto& birth : birth_map_)
    (*birth_map)[birth.first] = birth.second;
  for (const auto& death : death_map_)
    (*death_map)[death.first] = death.second;

  if (!kTrackParentChildLinks)
    return;

  for (const auto& parent_child : parent_child_set_)
    parent_child_set->insert(parent_child);
}

static void OptionallyInitializeAlternateTimer() {
  NowFunction* alternate_time_source = GetAlternateTimeSource();
  if (alternate_time_source)
    ThreadData::SetAlternateTimeSource(alternate_time_source);
}

bool ThreadData::Initialize() {
  if (!kTrackAllTaskObjects)
    return false;  // Not compiled in.
  if (status_ >= DEACTIVATED)
    return true;  // Someone else did the initialization.
  // Due to racy lazy initialization in tests, we'll need to recheck status_
  // after we acquire the lock.

  // Ensure that we don't double initialize tls.  We are called when single
  // threaded in the product, but some tests may be racy and lazy about our
  // initialization.
  base::AutoLock lock(*list_lock_.Pointer());
  if (status_ >= DEACTIVATED)
    return true;  // Someone raced in here and beat us.

  // Put an alternate timer in place if the environment calls for it, such as
  // for tracking TCMalloc allocations.  This insertion is idempotent, so we
  // don't mind if there is a race, and we'd prefer not to be in a lock while
  // doing this work.
  if (kAllowAlternateTimeSourceHandling)
    OptionallyInitializeAlternateTimer();

  // Perform the "real" TLS initialization now, and leave it intact through
  // process termination.
  if (!tls_index_.initialized()) {  // Testing may have initialized this.
    DCHECK_EQ(status_, UNINITIALIZED);
    tls_index_.Initialize(&ThreadData::OnThreadTermination);
    if (!tls_index_.initialized())
      return false;
  } else {
    // TLS was initialzed for us earlier.
    DCHECK_EQ(status_, DORMANT_DURING_TESTS);
  }

  // Incarnation counter is only significant to testing, as it otherwise will
  // never again change in this process.
  ++incarnation_counter_;

  // The lock is not critical for setting status_, but it doesn't hurt. It also
  // ensures that if we have a racy initialization, that we'll bail as soon as
  // we get the lock earlier in this method.
  status_ = kInitialStartupState;
  if (!kTrackParentChildLinks &&
      kInitialStartupState == PROFILING_CHILDREN_ACTIVE)
    status_ = PROFILING_ACTIVE;
  DCHECK(status_ != UNINITIALIZED);
  return true;
}

// static
bool ThreadData::InitializeAndSetTrackingStatus(Status status) {
  DCHECK_GE(status, DEACTIVATED);
  DCHECK_LE(status, PROFILING_CHILDREN_ACTIVE);

  if (!Initialize())  // No-op if already initialized.
    return false;  // Not compiled in.

  if (!kTrackParentChildLinks && status > DEACTIVATED)
    status = PROFILING_ACTIVE;
  status_ = status;
  return true;
}

// static
ThreadData::Status ThreadData::status() {
  return status_;
}

// static
bool ThreadData::TrackingStatus() {
  return status_ > DEACTIVATED;
}

// static
bool ThreadData::TrackingParentChildStatus() {
  return status_ >= PROFILING_CHILDREN_ACTIVE;
}

// static
void ThreadData::PrepareForStartOfRun(const Births* parent) {
  if (kTrackParentChildLinks && parent && status_ > PROFILING_ACTIVE) {
    ThreadData* current_thread_data = Get();
    if (current_thread_data)
      current_thread_data->parent_stack_.push(parent);
  }
}

// static
void ThreadData::SetAlternateTimeSource(NowFunction* now_function) {
  DCHECK(now_function);
  if (kAllowAlternateTimeSourceHandling)
    now_function_ = now_function;
}

// static
void ThreadData::EnableProfilerTiming() {
  base::subtle::NoBarrier_Store(&g_profiler_timing_enabled, ENABLED_TIMING);
}

// static
TrackedTime ThreadData::Now() {
  if (kAllowAlternateTimeSourceHandling && now_function_)
    return TrackedTime::FromMilliseconds((*now_function_)());
  if (kTrackAllTaskObjects && IsProfilerTimingEnabled() && TrackingStatus())
    return TrackedTime::Now();
  return TrackedTime();  // Super fast when disabled, or not compiled.
}

// static
void ThreadData::EnsureCleanupWasCalled(int major_threads_shutdown_count) {
  base::AutoLock lock(*list_lock_.Pointer());
  if (worker_thread_data_creation_count_ == 0)
    return;  // We haven't really run much, and couldn't have leaked.

  // TODO(jar): until this is working on XP, don't run the real test.
#if 0
  // Verify that we've at least shutdown/cleanup the major namesd threads.  The
  // caller should tell us how many thread shutdowns should have taken place by
  // now.
  CHECK_GT(cleanup_count_, major_threads_shutdown_count);
#endif
}

// static
void ThreadData::ShutdownSingleThreadedCleanup(bool leak) {
  // This is only called from test code, where we need to cleanup so that
  // additional tests can be run.
  // We must be single threaded... but be careful anyway.
  if (!InitializeAndSetTrackingStatus(DEACTIVATED))
    return;
  ThreadData* thread_data_list;
  {
    base::AutoLock lock(*list_lock_.Pointer());
    thread_data_list = all_thread_data_list_head_;
    all_thread_data_list_head_ = NULL;
    ++incarnation_counter_;
    // To be clean, break apart the retired worker list (though we leak them).
    while (first_retired_worker_) {
      ThreadData* worker = first_retired_worker_;
      CHECK_GT(worker->worker_thread_number_, 0);
      first_retired_worker_ = worker->next_retired_worker_;
      worker->next_retired_worker_ = NULL;
    }
  }

  // Put most global static back in pristine shape.
  worker_thread_data_creation_count_ = 0;
  cleanup_count_ = 0;
  tls_index_.Set(NULL);
  status_ = DORMANT_DURING_TESTS;  // Almost UNINITIALIZED.

  // To avoid any chance of racing in unit tests, which is the only place we
  // call this function, we may sometimes leak all the data structures we
  // recovered, as they may still be in use on threads from prior tests!
  if (leak) {
    ThreadData* thread_data = thread_data_list;
    while (thread_data) {
      ANNOTATE_LEAKING_OBJECT_PTR(thread_data);
      thread_data = thread_data->next();
    }
    return;
  }

  // When we want to cleanup (on a single thread), here is what we do.

  // Do actual recursive delete in all ThreadData instances.
  while (thread_data_list) {
    ThreadData* next_thread_data = thread_data_list;
    thread_data_list = thread_data_list->next();

    for (BirthMap::iterator it = next_thread_data->birth_map_.begin();
         next_thread_data->birth_map_.end() != it; ++it)
      delete it->second;  // Delete the Birth Records.
    delete next_thread_data;  // Includes all Death Records.
  }
}

//------------------------------------------------------------------------------
TaskStopwatch::TaskStopwatch()
    : wallclock_duration_ms_(0),
      current_thread_data_(NULL),
      excluded_duration_ms_(0),
      parent_(NULL) {
#if DCHECK_IS_ON()
  state_ = CREATED;
  child_ = NULL;
#endif
}

TaskStopwatch::~TaskStopwatch() {
#if DCHECK_IS_ON()
  DCHECK(state_ != RUNNING);
  DCHECK(child_ == NULL);
#endif
}

void TaskStopwatch::Start() {
#if DCHECK_IS_ON()
  DCHECK(state_ == CREATED);
  state_ = RUNNING;
#endif

  start_time_ = ThreadData::Now();

  current_thread_data_ = ThreadData::Get();
  if (!current_thread_data_)
    return;

  parent_ = current_thread_data_->current_stopwatch_;
#if DCHECK_IS_ON()
  if (parent_) {
    DCHECK(parent_->state_ == RUNNING);
    DCHECK(parent_->child_ == NULL);
    parent_->child_ = this;
  }
#endif
  current_thread_data_->current_stopwatch_ = this;
}

void TaskStopwatch::Stop() {
  const TrackedTime end_time = ThreadData::Now();
#if DCHECK_IS_ON()
  DCHECK(state_ == RUNNING);
  state_ = STOPPED;
  DCHECK(child_ == NULL);
#endif

  if (!start_time_.is_null() && !end_time.is_null()) {
    wallclock_duration_ms_ = (end_time - start_time_).InMilliseconds();
  }

  if (!current_thread_data_)
    return;

  DCHECK(current_thread_data_->current_stopwatch_ == this);
  current_thread_data_->current_stopwatch_ = parent_;
  if (!parent_)
    return;

#if DCHECK_IS_ON()
  DCHECK(parent_->state_ == RUNNING);
  DCHECK(parent_->child_ == this);
  parent_->child_ = NULL;
#endif
  parent_->excluded_duration_ms_ += wallclock_duration_ms_;
  parent_ = NULL;
}

TrackedTime TaskStopwatch::StartTime() const {
#if DCHECK_IS_ON()
  DCHECK(state_ != CREATED);
#endif

  return start_time_;
}

int32 TaskStopwatch::RunDurationMs() const {
#if DCHECK_IS_ON()
  DCHECK(state_ == STOPPED);
#endif

  return wallclock_duration_ms_ - excluded_duration_ms_;
}

ThreadData* TaskStopwatch::GetThreadData() const {
#if DCHECK_IS_ON()
  DCHECK(state_ != CREATED);
#endif

  return current_thread_data_;
}

//------------------------------------------------------------------------------
TaskSnapshot::TaskSnapshot() {
}

TaskSnapshot::TaskSnapshot(const BirthOnThread& birth,
                           const DeathData& death_data,
                           const std::string& death_thread_name)
    : birth(birth),
      death_data(death_data),
      death_thread_name(death_thread_name) {
}

TaskSnapshot::~TaskSnapshot() {
}

//------------------------------------------------------------------------------
// ParentChildPairSnapshot

ParentChildPairSnapshot::ParentChildPairSnapshot() {
}

ParentChildPairSnapshot::ParentChildPairSnapshot(
    const ThreadData::ParentChildPair& parent_child)
    : parent(*parent_child.first),
      child(*parent_child.second) {
}

ParentChildPairSnapshot::~ParentChildPairSnapshot() {
}

//------------------------------------------------------------------------------
// ProcessDataPhaseSnapshot

ProcessDataPhaseSnapshot::ProcessDataPhaseSnapshot() {
}

ProcessDataPhaseSnapshot::~ProcessDataPhaseSnapshot() {
}

//------------------------------------------------------------------------------
// ProcessDataPhaseSnapshot

ProcessDataSnapshot::ProcessDataSnapshot()
#if !defined(OS_NACL)
    : process_id(base::GetCurrentProcId()) {
#else
    : process_id(base::kNullProcessId) {
#endif
}

ProcessDataSnapshot::~ProcessDataSnapshot() {
}

}  // namespace tracked_objects
