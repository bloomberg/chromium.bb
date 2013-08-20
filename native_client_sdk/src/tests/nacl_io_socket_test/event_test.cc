/* Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "gtest/gtest.h"

#include "nacl_io/event_emitter.h"
#include "nacl_io/event_listener.h"
#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_proxy.h"
#include "nacl_io/kernel_wrap.h"


using namespace nacl_io;
using namespace sdk_util;

class EventEmitterTester : public MountNode {
 public:
  EventEmitterTester() : MountNode(NULL), event_status_(0), event_cnt_(0) {}

  void SetEventStatus(uint32_t bits) { event_status_ = bits; }
  uint32_t GetEventStatus() { return event_status_; }

  Error Ioctl(int request, char* arg) {
    event_status_ = static_cast<uint32_t>(request);
    return 0;
  }

  int GetType() { return S_IFSOCK; }
  int NumEvents() { return event_cnt_; }

 public:
  // Make this function public for testing
  void RaiseEvent(uint32_t events) {
    EventEmitter::RaiseEvent(events);
  }

    // Called after registering locally, but while lock is still held.
  void ChainRegisterEventInfo(const ScopedEventInfo& event) {
    event_cnt_++;
  }

  // Called before unregistering locally, but while lock is still held.
  void ChainUnregisterEventInfo(const ScopedEventInfo& event) {
    event_cnt_--;
  }

 protected:
  uint32_t event_status_;
  uint32_t event_cnt_;
};


const int MAX_EVENTS = 8;

// IDs for Emitters
const int ID_EMITTER = 5;
const int ID_LISTENER = 6;
const int ID_EMITTER_DUP = 7;

// Kernel Event values
const uint32_t KE_EXPECTED = 4;
const uint32_t KE_FILTERED = 2;
const uint32_t KE_NONE = 0;

// User Data values
const uint64_t USER_DATA_A = 1;
const uint64_t USER_DATA_B = 5;

// Timeout durations
const int TIMEOUT_IMMEDIATE = 0;
const int TIMEOUT_SHORT= 100;
const int TIMEOUT_LONG = 500;
const int TIMEOUT_NEVER = -1;
const int TIMEOUT_VERY_LONG = 1000;

// We subtract TIMEOUT_SLOP from the expected minimum timed due to rounding
// and clock drift converting between absolute and relative time.  This should
// only be 1 for Less Than, and 1 for rounding, but we use 10 since we don't
// care about real precision, aren't testing of the underlying
// implementations and don't want flakiness.
const int TIMEOUT_SLOP = 10;

TEST(EventTest, EmitterBasic) {
  ScopedRef<EventEmitterTester> emitter(new EventEmitterTester());
  ScopedRef<EventEmitter> null_emitter;

  ScopedEventListener listener(new EventListener);

  // Verify construction
  EXPECT_EQ(0, emitter->NumEvents());
  EXPECT_EQ(0, emitter->GetEventStatus());

  // Verify status
  emitter->SetEventStatus(KE_EXPECTED);
  EXPECT_EQ(KE_EXPECTED, emitter->GetEventStatus());

  // Fail to update or free an ID not in the set
  EXPECT_EQ(ENOENT, listener->Update(ID_EMITTER, KE_EXPECTED, USER_DATA_A));
  EXPECT_EQ(ENOENT, listener->Free(ID_EMITTER));

  // Fail to Track self
  EXPECT_EQ(EINVAL, listener->Track(ID_LISTENER,
                                    listener,
                                    KE_EXPECTED,
                                    USER_DATA_A));

  // Set the emitter filter and data
  EXPECT_EQ(0, listener->Track(ID_EMITTER, emitter, KE_EXPECTED, USER_DATA_A));
  EXPECT_EQ(1, emitter->NumEvents());

  // Fail to add the same ID
  EXPECT_EQ(EEXIST,
            listener->Track(ID_EMITTER, emitter, KE_EXPECTED, USER_DATA_A));
  EXPECT_EQ(1, emitter->NumEvents());

  int event_cnt = 0;
  EventData ev[MAX_EVENTS];

  // Do not allow a wait with a zero events count.
  EXPECT_EQ(EINVAL, listener->Wait(ev, 0, TIMEOUT_IMMEDIATE, &event_cnt));

  // Do not allow a wait with a negative events count.
  EXPECT_EQ(EINVAL, listener->Wait(ev, -1, TIMEOUT_IMMEDIATE, &event_cnt));

  // Do not allow a wait with a NULL EventData pointer
  EXPECT_EQ(EFAULT,
            listener->Wait(NULL, MAX_EVENTS, TIMEOUT_IMMEDIATE, &event_cnt));

  // Return with no events if the Emitter has no signals set.
  memset(ev, 0, sizeof(ev));
  event_cnt = 100;
  emitter->SetEventStatus(KE_NONE);
  EXPECT_EQ(0, listener->Wait(ev, MAX_EVENTS, TIMEOUT_IMMEDIATE, &event_cnt));
  EXPECT_EQ(0, event_cnt);

  // Return with no events if the Emitter has a filtered signals set.
  memset(ev, 0, sizeof(ev));
  event_cnt = 100;
  emitter->SetEventStatus(KE_FILTERED);
  EXPECT_EQ(0, listener->Wait(ev, MAX_EVENTS, TIMEOUT_IMMEDIATE, &event_cnt));
  EXPECT_EQ(0, event_cnt);

  // Return with one event if the Emitter has the expected signal set.
  memset(ev, 0, sizeof(ev));
  event_cnt = 100;
  emitter->SetEventStatus(KE_EXPECTED);
  EXPECT_EQ(0, listener->Wait(ev, MAX_EVENTS, TIMEOUT_IMMEDIATE, &event_cnt));
  EXPECT_EQ(1, event_cnt);
  EXPECT_EQ(USER_DATA_A, ev[0].user_data);
  EXPECT_EQ(KE_EXPECTED, ev[0].events);

  // Return with one event containing only the expected signal.
  memset(ev, 0, sizeof(ev));
  event_cnt = 100;
  emitter->SetEventStatus(KE_EXPECTED | KE_FILTERED);
  EXPECT_EQ(0, listener->Wait(ev, MAX_EVENTS, TIMEOUT_IMMEDIATE, &event_cnt));
  EXPECT_EQ(1, event_cnt);
  EXPECT_EQ(USER_DATA_A, ev[0].user_data);
  EXPECT_EQ(KE_EXPECTED, ev[0].events);

  // Change the USER_DATA on an existing event
  EXPECT_EQ(0, listener->Update(ID_EMITTER, KE_EXPECTED, USER_DATA_B));

  // Return with one event signaled with the alternate USER DATA
  memset(ev, 0, sizeof(ev));
  event_cnt = 100;
  emitter->SetEventStatus(KE_EXPECTED | KE_FILTERED);
  EXPECT_EQ(0, listener->Wait(ev, MAX_EVENTS, 0, &event_cnt));
  EXPECT_EQ(1, event_cnt);
  EXPECT_EQ(USER_DATA_B, ev[0].user_data);
  EXPECT_EQ(KE_EXPECTED, ev[0].events);

  // Reset the USER_DATA.
  EXPECT_EQ(0, listener->Update(ID_EMITTER, KE_EXPECTED, USER_DATA_A));

  // Support adding a DUP.
  EXPECT_EQ(0, listener->Track(ID_EMITTER_DUP,
                               emitter,
                               KE_EXPECTED,
                               USER_DATA_A));
  EXPECT_EQ(2, emitter->NumEvents());

  // Return unsignaled.
  memset(ev, 0, sizeof(ev));
  emitter->SetEventStatus(KE_NONE);
  event_cnt = 100;
  EXPECT_EQ(0, listener->Wait(ev, MAX_EVENTS, TIMEOUT_IMMEDIATE, &event_cnt));
  EXPECT_EQ(0, event_cnt);

  // Return with two event signaled with expected data.
  memset(ev, 0, sizeof(ev));
  emitter->SetEventStatus(KE_EXPECTED);
  event_cnt = 100;
  EXPECT_EQ(0, listener->Wait(ev, MAX_EVENTS, TIMEOUT_IMMEDIATE, &event_cnt));
  EXPECT_EQ(2, event_cnt);
  EXPECT_EQ(USER_DATA_A, ev[0].user_data);
  EXPECT_EQ(KE_EXPECTED, ev[0].events);
  EXPECT_EQ(USER_DATA_A, ev[1].user_data);
  EXPECT_EQ(KE_EXPECTED, ev[1].events);
}

long Duration(struct timeval* start, struct timeval* end) {
  if (start->tv_usec > end->tv_usec) {
    end->tv_sec -= 1;
    end->tv_usec += 1000000;
  }
  long cur_time = 1000 * (end->tv_sec - start->tv_sec);
  cur_time += (end->tv_usec - start->tv_usec) / 1000;
  return cur_time;
}


// Run a timed wait, and return the average of 8 iterations to reduce
// chance of false negative on outlier.
const int TRIES_TO_AVERAGE = 8;
bool TimedListen(ScopedEventListener& listen,
                 EventData* ev,
                 int ev_max,
                 int ev_expect,
                 int ms_wait,
                 long* duration) {

  struct timeval start;
  struct timeval end;
  long total_time = 0;

  for (int a=0; a < TRIES_TO_AVERAGE; a++) {
    gettimeofday(&start, NULL);

    int signaled;

    EXPECT_EQ(0, listen->Wait(ev, ev_max, ms_wait, &signaled));
    EXPECT_EQ(signaled, ev_expect);

    if (signaled != ev_expect) {
      return false;
    }

    gettimeofday(&end, NULL);

    long cur_time = Duration(&start, &end);
    total_time += cur_time;
  }

  *duration = total_time / TRIES_TO_AVERAGE;
  return true;
}


// NOTE:  These timing tests are potentially flaky, the real test is
// for the zero timeout should be, has the ConditionVariable been waited on?
// Once we provide a debuggable SimpleCond and SimpleLock we can actually test
// the correct thing.

// Normal scheduling would expect us to see ~10ms accuracy, but we'll
// use a much bigger number (yet smaller than the MAX_MS_TIMEOUT).
const int SCHEDULING_GRANULARITY = 100;

const int EXPECT_ONE_EVENT = 1;
const int EXPECT_NO_EVENT = 0;

TEST(EventTest, EmitterTimeout) {
  ScopedRef<EventEmitterTester> emitter(new EventEmitterTester());
  ScopedEventListener listener(new EventListener());
  long duration;

  EventData ev[MAX_EVENTS];
  memset(ev, 0, sizeof(ev));
  EXPECT_EQ(0, listener->Track(ID_EMITTER, emitter, KE_EXPECTED, USER_DATA_A));

  // Return immediately when emitter is signaled, with no timeout
  emitter->SetEventStatus(KE_EXPECTED);
  memset(ev, 0, sizeof(ev));
  EXPECT_TRUE(TimedListen(listener, ev, MAX_EVENTS, EXPECT_ONE_EVENT,
                          TIMEOUT_IMMEDIATE, &duration));
  EXPECT_EQ(USER_DATA_A, ev[0].user_data);
  EXPECT_EQ(KE_EXPECTED, ev[0].events);
  EXPECT_EQ(0, duration);

  // Return immediately when emitter is signaled, even with timeout
  emitter->SetEventStatus(KE_EXPECTED);
  memset(ev, 0, sizeof(ev));
  EXPECT_TRUE(TimedListen(listener, ev, MAX_EVENTS, EXPECT_ONE_EVENT,
                          TIMEOUT_LONG, &duration));
  EXPECT_EQ(USER_DATA_A, ev[0].user_data);
  EXPECT_EQ(KE_EXPECTED, ev[0].events);
  EXPECT_GT(SCHEDULING_GRANULARITY, duration);

  // Return immediately if Emiiter is already signaled when blocking forever.
  emitter->SetEventStatus(KE_EXPECTED);
  memset(ev, 0, sizeof(ev));
  EXPECT_TRUE(TimedListen(listener, ev, MAX_EVENTS, EXPECT_ONE_EVENT,
                          TIMEOUT_NEVER, &duration));
  EXPECT_EQ(USER_DATA_A, ev[0].user_data);
  EXPECT_EQ(KE_EXPECTED, ev[0].events);
  EXPECT_GT(SCHEDULING_GRANULARITY, duration);

  // Return immediately if Emitter is no signaled when not blocking.
  emitter->SetEventStatus(KE_NONE);
  memset(ev, 0, sizeof(ev));
  EXPECT_TRUE(TimedListen(listener, ev, MAX_EVENTS, EXPECT_NO_EVENT,
                          TIMEOUT_IMMEDIATE, &duration));
  EXPECT_EQ(0, duration);

  // Wait TIMEOUT_LONG if the emitter is not in a signaled state.
  emitter->SetEventStatus(KE_NONE);
  memset(ev, 0, sizeof(ev));
  EXPECT_TRUE(TimedListen(listener, ev, MAX_EVENTS, EXPECT_NO_EVENT,
                          TIMEOUT_LONG, &duration));
  EXPECT_LT(TIMEOUT_LONG - TIMEOUT_SLOP, duration);
  EXPECT_GT(TIMEOUT_LONG + SCHEDULING_GRANULARITY, duration);
}

struct SignalInfo {
  EventEmitterTester* em;
  unsigned int ms_wait;
  uint32_t events;
};

void *SignalEmitter(void *ptr) {
  SignalInfo* info = (SignalInfo*) ptr;
  struct timespec ts;
  ts.tv_sec = 0;
  ts.tv_nsec = info->ms_wait * 1000000;

  nanosleep(&ts, NULL);

  info->em->RaiseEvent(info->events);
  return NULL;
}

TEST(EventTest, EmitterSignalling) {
  ScopedRef<EventEmitterTester> emitter(new EventEmitterTester());
  ScopedEventListener listener(new EventListener);

  SignalInfo siginfo;
  struct timeval start;
  struct timeval end;
  long duration;

  EventData ev[MAX_EVENTS];
  memset(ev, 0, sizeof(ev));
  EXPECT_EQ(0, listener->Track(ID_EMITTER, emitter, KE_EXPECTED, USER_DATA_A));

  // Setup another thread to wait 1/4 of the max time, and signal both
  // an expected, and unexpected value.
  siginfo.em = emitter.get();
  siginfo.ms_wait = TIMEOUT_SHORT;
  siginfo.events = KE_EXPECTED | KE_FILTERED;
  pthread_t tid;
  pthread_create(&tid, NULL, SignalEmitter, &siginfo);

  // Wait for the signal from the other thread and time it.
  gettimeofday(&start, NULL);
  int cnt = 0;
  EXPECT_EQ(0, listener->Wait(ev, MAX_EVENTS, TIMEOUT_VERY_LONG, &cnt));
  EXPECT_EQ(1, cnt);
  gettimeofday(&end, NULL);

  // Verify the wait duration, and that we only recieved the expected signal.
  duration = Duration(&start, &end);
  EXPECT_GT(TIMEOUT_SHORT + SCHEDULING_GRANULARITY, duration);
  EXPECT_LT(TIMEOUT_SHORT - TIMEOUT_SLOP, duration);
  EXPECT_EQ(USER_DATA_A, ev[0].user_data);
  EXPECT_EQ(KE_EXPECTED, ev[0].events);
}


namespace {

class KernelProxyPolling : public KernelProxy {
 public:
  virtual int socket(int domain, int type, int protocol) {
    ScopedMount mnt;
    ScopedMountNode node(new EventEmitterTester());
    ScopedKernelHandle handle(new KernelHandle(mnt, node));

    Error error = handle->Init(0);
    if (error) {
      errno = error;
      return -1;
    }

    return AllocateFD(handle);
  }
};

class KernelProxyPollingTest : public ::testing::Test {
 public:
  KernelProxyPollingTest() : kp_(new KernelProxyPolling) {
    ki_init(kp_);
  }

  ~KernelProxyPollingTest() {
    ki_uninit();
    delete kp_;
  }

  KernelProxyPolling* kp_;
};

}  // namespace


#define SOCKET_CNT 4
void SetFDs(fd_set* set, int* fds) {
  FD_ZERO(set);

  FD_SET(0, set);
  FD_SET(1, set);
  FD_SET(2, set);

  for (int index = 0; index < SOCKET_CNT; index++)
    FD_SET(fds[index], set);
}

TEST_F(KernelProxyPollingTest, Select) {
  int fds[SOCKET_CNT];

  fd_set rd_set;
  fd_set wr_set;

  FD_ZERO(&rd_set);
  FD_ZERO(&wr_set);

  FD_SET(0, &rd_set);
  FD_SET(1, &rd_set);
  FD_SET(2, &rd_set);

  FD_SET(0, &wr_set);
  FD_SET(1, &wr_set);
  FD_SET(2, &wr_set);

  // Expect normal files to select as read, write, and error
  int cnt = select(4, &rd_set, &rd_set, &rd_set, NULL);
  EXPECT_EQ(3 * 3, cnt);
  EXPECT_NE(0, FD_ISSET(0, &rd_set));
  EXPECT_NE(0, FD_ISSET(1, &rd_set));
  EXPECT_NE(0, FD_ISSET(2, &rd_set));

  for (int index = 0 ; index < SOCKET_CNT; index++) {
    fds[index] = socket(0, 0, 0);
    EXPECT_NE(-1, fds[index]);
  }

  // Highest numbered fd
  const int fdnum = fds[SOCKET_CNT - 1] + 1;

  // Expect only the normal files to select
  SetFDs(&rd_set, fds);
  cnt = select(fds[SOCKET_CNT-1] + 1, &rd_set, NULL, NULL, NULL);
  EXPECT_EQ(3, cnt);
  EXPECT_NE(0, FD_ISSET(0, &rd_set));
  EXPECT_NE(0, FD_ISSET(1, &rd_set));
  EXPECT_NE(0, FD_ISSET(2, &rd_set));
  for (int index = 0 ; index < SOCKET_CNT; index++) {
    EXPECT_EQ(0, FD_ISSET(fds[index], &rd_set));
  }

  // Poke one of the pollable nodes to be READ ready
  ioctl(fds[0], POLLIN, NULL);

  // Expect normal files to be read/write and one pollable node to be read.
  SetFDs(&rd_set, fds);
  SetFDs(&wr_set, fds);
  cnt = select(fdnum, &rd_set, &wr_set, NULL, NULL);
  EXPECT_EQ(7, cnt);
  EXPECT_NE(0, FD_ISSET(fds[0], &rd_set));
  EXPECT_EQ(0, FD_ISSET(fds[0], &wr_set));
}


