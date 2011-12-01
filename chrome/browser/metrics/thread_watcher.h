// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines a WatchDog thread that monitors the responsiveness of other
// browser threads like UI, IO, DB, FILE and CACHED threads. It also defines
// ThreadWatcher class which performs health check on threads that would like to
// be watched. This file also defines ThreadWatcherList class that has list of
// all active ThreadWatcher objects.
//
// ThreadWatcher class sends ping message to the watched thread and the watched
// thread responds back with a pong message. It uploads response time
// (difference between ping and pong times) as a histogram.
//
// TODO(raman): ThreadWatcher can detect hung threads. If a hung thread is
// detected, we should probably just crash, and allow the crash system to gather
// then stack trace.
//
// Example Usage:
//
//   The following is an example for watching responsiveness of watched (IO)
//   thread. |sleep_time| specifies how often ping messages have to be sent to
//   watched (IO) thread. |unresponsive_time| is the wait time after ping
//   message is sent, to check if we have received pong message or not.
//   |unresponsive_threshold| specifies the number of unanswered ping messages
//   after which watched (IO) thread is considered as not responsive.
//   |crash_on_hang| specifies if we want to crash the browser when the watched
//   (IO) thread has become sufficiently unresponsive, while other threads are
//   sufficiently responsive. |live_threads_threshold| specifies the number of
//   browser threads that are to be responsive when we want to crash the browser
//   because of hung watched (IO) thread.
//
//   base::TimeDelta sleep_time = base::TimeDelta::FromSeconds(5);
//   base::TimeDelta unresponsive_time = base::TimeDelta::FromSeconds(10);
//   uint32 unresponsive_threshold = ThreadWatcherList::kUnresponsiveCount;
//   bool crash_on_hang = false;
//   uint32 live_threads_threshold = ThreadWatcherList::kLiveThreadsThreshold;
//   ThreadWatcher::StartWatching(
//       BrowserThread::IO, "IO", sleep_time, unresponsive_time,
//       unresponsive_threshold, crash_on_hang, live_threads_threshold);

#ifndef CHROME_BROWSER_METRICS_THREAD_WATCHER_H_
#define CHROME_BROWSER_METRICS_THREAD_WATCHER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/threading/watchdog.h"
#include "base/time.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class CustomThreadWatcher;
class StartupTimeBomb;
class ThreadWatcherList;
class ThreadWatcherObserver;

// This class performs health check on threads that would like to be watched.
class ThreadWatcher {
 public:
  // base::Bind supports methods with up to 6 parameters. WatchingParams is used
  // as a workaround that limitation for invoking ThreadWatcher::StartWatching.
  struct WatchingParams {
    const content::BrowserThread::ID& thread_id;
    const std::string& thread_name;
    const base::TimeDelta& sleep_time;
    const base::TimeDelta& unresponsive_time;
    uint32 unresponsive_threshold;
    bool crash_on_hang;
    uint32 live_threads_threshold;

    WatchingParams(const content::BrowserThread::ID& thread_id_in,
                   const std::string& thread_name_in,
                   const base::TimeDelta& sleep_time_in,
                   const base::TimeDelta& unresponsive_time_in,
                   uint32 unresponsive_threshold_in,
                   bool crash_on_hang_in,
                   uint32 live_threads_threshold_in)
        : thread_id(thread_id_in),
          thread_name(thread_name_in),
          sleep_time(sleep_time_in),
          unresponsive_time(unresponsive_time_in),
          unresponsive_threshold(unresponsive_threshold_in),
          crash_on_hang(crash_on_hang_in),
          live_threads_threshold(live_threads_threshold_in) {
    }
  };

  // This method starts performing health check on the given |thread_id|. It
  // will create ThreadWatcher object for the given |thread_id|, |thread_name|.
  // |sleep_time| is the wait time between ping messages. |unresponsive_time| is
  // the wait time after ping message is sent, to check if we have received pong
  // message or not. |unresponsive_threshold| is used to determine if the thread
  // is responsive or not. The watched thread is considered unresponsive if it
  // hasn't responded with a pong message for |unresponsive_threshold| number of
  // ping messages. |crash_on_hang| specifies if browser should be crashed when
  // the watched thread is unresponsive. |live_threads_threshold| specifies the
  // number of browser threads that are to be responsive when we want to crash
  // the browser and watched thread has become sufficiently unresponsive. It
  // will register that ThreadWatcher object and activate the thread watching of
  // the given thread_id.
  static void StartWatching(const WatchingParams& params);

  // Return the |thread_id_| of the thread being watched.
  content::BrowserThread::ID thread_id() const { return thread_id_; }

  // Return the name of the thread being watched.
  std::string thread_name() const { return thread_name_; }

  // Return the sleep time between ping messages to be sent to the thread.
  base::TimeDelta sleep_time() const { return sleep_time_; }

  // Return the the wait time to check the responsiveness of the thread.
  base::TimeDelta unresponsive_time() const { return unresponsive_time_; }

  // Returns true if we are montioring the thread.
  bool active() const { return active_; }

  // Returns |ping_time_| (used by unit tests).
  base::TimeTicks ping_time() const { return ping_time_; }

  // Returns |ping_sequence_number_| (used by unit tests).
  uint64 ping_sequence_number() const { return ping_sequence_number_; }

 protected:
  // Construct a ThreadWatcher for the given |thread_id|. |sleep_time| is the
  // wait time between ping messages. |unresponsive_time| is the wait time after
  // ping message is sent, to check if we have received pong message or not.
  explicit ThreadWatcher(const WatchingParams& params);

  virtual ~ThreadWatcher();

  // This method activates the thread watching which starts ping/pong messaging.
  virtual void ActivateThreadWatching();

  // This method de-activates the thread watching and revokes all tasks.
  virtual void DeActivateThreadWatching();

  // This will ensure that the watching is actively taking place, and awaken
  // (i.e., post a PostPingMessage()) if the watcher has stopped pinging due to
  // lack of user activity. It will also reset |ping_count_| to
  // |unresponsive_threshold_|.
  virtual void WakeUp();

  // This method records when ping message was sent and it will Post a task
  // (OnPingMessage()) to the watched thread that does nothing but respond with
  // OnPongMessage(). It also posts a task (OnCheckResponsiveness()) to check
  // responsiveness of monitored thread that would be called after waiting
  // |unresponsive_time_|.
  // This method is accessible on WatchDogThread.
  virtual void PostPingMessage();

  // This method handles a Pong Message from watched thread. It will track the
  // response time (pong time minus ping time) via histograms. It posts a
  // PostPingMessage() task that would be called after waiting |sleep_time_|. It
  // increments |ping_sequence_number_| by 1.
  // This method is accessible on WatchDogThread.
  virtual void OnPongMessage(uint64 ping_sequence_number);

  // This method will determine if the watched thread is responsive or not. If
  // the latest |ping_sequence_number_| is not same as the
  // |ping_sequence_number| that is passed in, then we can assume that watched
  // thread has responded with a pong message.
  // This method is accessible on WatchDogThread.
  virtual void OnCheckResponsiveness(uint64 ping_sequence_number);

  // Set by OnCheckResponsiveness when it determines if the watched thread is
  // responsive or not.
  bool responsive_;

 private:
  friend class ThreadWatcherList;
  friend class CustomThreadWatcher;

  // Allow tests to access our innards for testing purposes.
  FRIEND_TEST_ALL_PREFIXES(ThreadWatcherTest, Registration);
  FRIEND_TEST_ALL_PREFIXES(ThreadWatcherTest, ThreadResponding);
  FRIEND_TEST_ALL_PREFIXES(ThreadWatcherTest, ThreadNotResponding);
  FRIEND_TEST_ALL_PREFIXES(ThreadWatcherTest, MultipleThreadsResponding);
  FRIEND_TEST_ALL_PREFIXES(ThreadWatcherTest, MultipleThreadsNotResponding);

  // Post constructor initialization.
  void Initialize();

  // Watched thread does nothing except post callback_task to the WATCHDOG
  // Thread. This method is called on watched thread.
  static void OnPingMessage(const content::BrowserThread::ID& thread_id,
                            const base::Closure& callback_task);

  // This method resets |unresponsive_count_| to zero because watched thread is
  // responding to the ping message with a pong message.
  void ResetHangCounters();

  // This method records watched thread is not responding to the ping message.
  // It increments |unresponsive_count_| by 1.
  void GotNoResponse();

  // This method returns true if the watched thread has not responded with a
  // pong message for |unresponsive_threshold_| number of ping messages.
  bool IsVeryUnresponsive();

  // The |thread_id_| of the thread being watched. Only one instance can exist
  // for the given |thread_id_| of the thread being watched.
  const content::BrowserThread::ID thread_id_;

  // The name of the thread being watched.
  const std::string thread_name_;

  // Used to post messages to watched thread.
  scoped_refptr<base::MessageLoopProxy> watched_loop_;

  // It is the sleep time between the receipt of a pong message back, and the
  // sending of another ping message.
  const base::TimeDelta sleep_time_;

  // It is the duration from sending a ping message, until we check status to be
  // sure a pong message has been returned.
  const base::TimeDelta unresponsive_time_;

  // This is the last time when ping message was sent.
  base::TimeTicks ping_time_;

  // This is the last time when we got pong message.
  base::TimeTicks pong_time_;

  // This is the sequence number of the next ping for which there is no pong. If
  // the instance is sleeping, then it will be the sequence number for the next
  // ping.
  uint64 ping_sequence_number_;

  // This is set to true if thread watcher is watching.
  bool active_;

  // The counter tracks least number of ping messages that will be sent to
  // watched thread before the ping-pong mechanism will go into an extended
  // sleep. If this value is zero, then the mechanism is in an extended sleep,
  // and awaiting some observed user action before continuing.
  int ping_count_;

  // Histogram that keeps track of response times for the watched thread.
  base::Histogram* response_time_histogram_;

  // Histogram that keeps track of unresponsive time since the last pong message
  // when we got no response (GotNoResponse()) from the watched thread.
  base::Histogram* unresponsive_time_histogram_;

  // Histogram that keeps track of how many threads are responding when we got
  // no response (GotNoResponse()) from the watched thread.
  base::Histogram* responsive_count_histogram_;

  // Histogram that keeps track of how many threads are not responding when we
  // got no response (GotNoResponse()) from the watched thread. Count includes
  // the thread that got no response.
  base::Histogram* unresponsive_count_histogram_;

  // This counter tracks the unresponsiveness of watched thread. If this value
  // is zero then watched thread has responded with a pong message. This is
  // incremented by 1 when we got no response (GotNoResponse()) from the watched
  // thread.
  uint32 unresponsive_count_;

  // This is set to true when we would have crashed the browser because the
  // watched thread hasn't responded at least |unresponsive_threshold_| times.
  // It is reset to false when watched thread responds with a pong message.
  bool hung_processing_complete_;

  // This is used to determine if the watched thread is responsive or not. If
  // watched thread's |unresponsive_count_| is greater than or equal to
  // |unresponsive_threshold_| then we would consider it as unresponsive.
  uint32 unresponsive_threshold_;

  // This is set to true if we want to crash the browser when the watched thread
  // has become sufficiently unresponsive, while other threads are sufficiently
  // responsive.
  bool crash_on_hang_;

  // This specifies the number of browser threads that are to be responsive when
  // we want to crash the browser because watched thread has become sufficiently
  // unresponsive.
  uint32 live_threads_threshold_;

  // We use this factory to create callback tasks for ThreadWatcher object. We
  // use this during ping-pong messaging between WatchDog thread and watched
  // thread.
  base::WeakPtrFactory<ThreadWatcher> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ThreadWatcher);
};

// Class with a list of all active thread watchers.  A thread watcher is active
// if it has been registered, which includes determing the histogram name. This
// class provides utility functions to start and stop watching all browser
// threads. Only one instance of this class exists.
class ThreadWatcherList {
 public:
  // A map from BrowserThread to the actual instances.
  typedef std::map<content::BrowserThread::ID, ThreadWatcher*> RegistrationList;

  // This method posts a task on WatchDogThread to start watching all browser
  // threads.
  // This method is accessible on UI thread.
  static void StartWatchingAll(const CommandLine& command_line);

  // This method posts a task on WatchDogThread to RevokeAll tasks and to
  // deactive thread watching of other threads and tell NotificationService to
  // stop calling Observe.
  // This method is accessible on UI thread.
  static void StopWatchingAll();

  // Register() stores a pointer to the given ThreadWatcher in a global map.
  static void Register(ThreadWatcher* watcher);

  // This method returns true if the ThreadWatcher object is registerd.
  static bool IsRegistered(const content::BrowserThread::ID thread_id);

  // This method returns number of responsive and unresponsive watched threads.
  static void GetStatusOfThreads(uint32* responding_thread_count,
                                 uint32* unresponding_thread_count);

  // This will ensure that the watching is actively taking place, and awaken
  // all thread watchers that are registered.
  static void WakeUpAll();

 private:
  // Allow tests to access our innards for testing purposes.
  friend class CustomThreadWatcher;
  friend class ThreadWatcherTest;
  FRIEND_TEST_ALL_PREFIXES(ThreadWatcherTest, CommandLineArgs);

  // This singleton holds the global list of registered ThreadWatchers.
  ThreadWatcherList();

  // Destructor deletes all registered ThreadWatcher instances.
  virtual ~ThreadWatcherList();

  // Parses the command line to get |unresponsive_threshold| from
  // switches::kCrashOnHangSeconds, |crash_on_hang_thread_names| from
  // switches::kCrashOnHangThreads and |live_threads_threshold| from
  // switches::kCrashOnLive. |crash_on_hang_thread_names| is the set of watched
  // thread's names that are to be crashed if they are not responding.
  static void ParseCommandLine(
      const CommandLine& command_line,
      uint32* unresponsive_threshold,
      std::set<std::string>* crash_on_hang_thread_names,
      uint32* live_threads_threshold);

  // This constructs the |ThreadWatcherList| singleton and starts watching
  // browser threads by calling StartWatching() on each browser thread that is
  // watched. It disarms StartupTimeBomb.
  static void InitializeAndStartWatching(
      uint32 unresponsive_threshold,
      const std::set<std::string>& crash_on_hang_thread_names,
      uint32 live_threads_threshold);

  // This method calls ThreadWatcher::StartWatching() to perform health check on
  // the given |thread_id|.
  static void StartWatching(
      const content::BrowserThread::ID& thread_id,
      const std::string& thread_name,
      const base::TimeDelta& sleep_time,
      const base::TimeDelta& unresponsive_time,
      uint32 unresponsive_threshold,
      const std::set<std::string>& crash_on_hang_thread_names,
      uint32 live_threads_threshold);

  // Delete all thread watcher objects and remove them from global map. It also
  // deletes |g_thread_watcher_list_|.
  static void DeleteAll();

  // The Find() method can be used to test to see if a given ThreadWatcher was
  // already registered, or to retrieve a pointer to it from the global map.
  static ThreadWatcher* Find(const content::BrowserThread::ID& thread_id);

  // The singleton of this class and is used to keep track of information about
  // threads that are being watched.
  static ThreadWatcherList* g_thread_watcher_list_;

  // This is the wait time between ping messages.
  static const int kSleepSeconds;

  // This is the wait time after ping message is sent, to check if we have
  // received pong message or not.
  static const int kUnresponsiveSeconds;

  // Default values for |unresponsive_threshold|.
  static const int kUnresponsiveCount;

  // Default values for |live_threads_threshold|.
  static const int kLiveThreadsThreshold;

  // Map of all registered watched threads, from thread_id to ThreadWatcher.
  RegistrationList registered_;

  DISALLOW_COPY_AND_ASSIGN(ThreadWatcherList);
};

// This class ensures that the thread watching is actively taking place. Only
// one instance of this class exists.
class ThreadWatcherObserver : public content::NotificationObserver {
 public:
  // Registers |g_thread_watcher_observer_| as the Notifications observer.
  // |wakeup_interval| specifies how often to wake up thread watchers. This
  // method is accessible on UI thread.
  static void SetupNotifications(const base::TimeDelta& wakeup_interval);

  // Removes all ints from |registrar_| and deletes
  // |g_thread_watcher_observer_|. This method is accessible on UI thread.
  static void RemoveNotifications();

 private:
  // Constructor of |g_thread_watcher_observer_| singleton.
  explicit ThreadWatcherObserver(const base::TimeDelta& wakeup_interval);

  // Destructor of |g_thread_watcher_observer_| singleton.
  virtual ~ThreadWatcherObserver();

  // This ensures all thread watchers are active because there is some user
  // activity. It will wake up all thread watchers every |wakeup_interval_|
  // seconds. This is the implementation of content::NotificationObserver. When
  // a matching notification is posted to the notification service, this method
  // is called.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // The singleton of this class.
  static ThreadWatcherObserver* g_thread_watcher_observer_;

  // The registrar that holds ints to be observed.
  content::NotificationRegistrar registrar_;

  // This is the last time when woke all thread watchers up.
  base::TimeTicks last_wakeup_time_;

  // It is the time interval between wake up calls to thread watchers.
  const base::TimeDelta wakeup_interval_;

  DISALLOW_COPY_AND_ASSIGN(ThreadWatcherObserver);
};

// Class for WatchDogThread and in its Init method, we start watching UI, IO,
// DB, FILE, CACHED threads.
class WatchDogThread : public base::Thread {
 public:
  // Constructor.
  WatchDogThread();

  // Destroys the thread and stops the thread.
  virtual ~WatchDogThread();

  // Callable on any thread.  Returns whether you're currently on a
  // WatchDogThread.
  static bool CurrentlyOnWatchDogThread();

  // These are the same methods in message_loop.h, but are guaranteed to either
  // get posted to the MessageLoop if it's still alive, or be deleted otherwise.
  // They return true iff the watchdog thread existed and the task was posted.
  // Note that even if the task is posted, there's no guarantee that it will
  // run, since the target thread may already have a Quit message in its queue.
  static bool PostTask(const tracked_objects::Location& from_here,
                       const base::Closure& task);
  static bool PostDelayedTask(const tracked_objects::Location& from_here,
                              const base::Closure& task,
                              int64 delay_ms);

 protected:
  virtual void Init() OVERRIDE;
  virtual void CleanUp() OVERRIDE;

 private:
  static bool PostTaskHelper(
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      int64 delay_ms);

  DISALLOW_COPY_AND_ASSIGN(WatchDogThread);
};

// This is a wrapper class for getting the crash dumps of the hangs during
// startup.
class StartupTimeBomb {
 public:
  // This singleton is instantiated when the browser process is launched.
  StartupTimeBomb();

  // Destructor disarm's startup_watchdog_ (if it is arm'ed) so that alarm
  // doesn't go off.
  ~StartupTimeBomb();

  // Constructs |startup_watchdog_| which spawns a thread and starts timer.
  // |duration| specifies how long |startup_watchdog_| will wait before it
  // calls alarm.
  void Arm(const base::TimeDelta& duration);

  // Disarms |startup_watchdog_| thread and then deletes it which stops the
  // Watchdog thread.
  void Disarm();

  // Disarms |g_startup_timebomb_|.
  static void DisarmStartupTimeBomb();

 private:
  // The singleton of this class.
  static StartupTimeBomb* g_startup_timebomb_;

  // Watches for hangs during startup until it is disarm'ed.
  base::Watchdog* startup_watchdog_;

  // The |thread_id_| on which this object is constructed.
  const base::PlatformThreadId thread_id_;

  DISALLOW_COPY_AND_ASSIGN(StartupTimeBomb);
};

// This is a wrapper class for detecting hangs during shutdown.
class ShutdownWatcherHelper {
 public:
  // Create an empty holder for |shutdown_watchdog_|.
  ShutdownWatcherHelper();

  // Destructor disarm's shutdown_watchdog_ so that alarm doesn't go off.
  ~ShutdownWatcherHelper();

  // Constructs ShutdownWatchDogThread which spawns a thread and starts timer.
  // |duration| specifies how long it will wait before it calls alarm.
  void Arm(const base::TimeDelta& duration);

 private:
  // shutdown_watchdog_ watches for hangs during shutdown.
  base::Watchdog* shutdown_watchdog_;

  // The |thread_id_| on which this object is constructed.
  const base::PlatformThreadId thread_id_;

  DISALLOW_COPY_AND_ASSIGN(ShutdownWatcherHelper);
};

// DISABLE_RUNNABLE_METHOD_REFCOUNT is a convenience macro for disabling
// refcounting of ThreadWatcher and ThreadWatcherList classes.
DISABLE_RUNNABLE_METHOD_REFCOUNT(ThreadWatcher);
DISABLE_RUNNABLE_METHOD_REFCOUNT(ThreadWatcherList);

#endif  // CHROME_BROWSER_METRICS_THREAD_WATCHER_H_
