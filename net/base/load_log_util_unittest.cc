// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/load_log_unittest.h"
#include "net/base/load_log_util.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

TEST(LoadLogUtilTest, Basic) {
  scoped_refptr<LoadLog> log(new LoadLog(10));

  log->Add(LoadLog::Entry(MakeTime(1),
                          LoadLog::Event(LoadLog::TYPE_HOST_RESOLVER_IMPL,
                                         LoadLog::PHASE_BEGIN)));
  log->Add(
      LoadLog::Entry(
          MakeTime(5),
          LoadLog::Event(LoadLog::TYPE_HOST_RESOLVER_IMPL_OBSERVER_ONSTART,
                         LoadLog::PHASE_BEGIN)));
  log->Add(
       LoadLog::Entry(
           MakeTime(8),
           LoadLog::Event(LoadLog::TYPE_HOST_RESOLVER_IMPL_OBSERVER_ONSTART,
                          LoadLog::PHASE_END)));

  log->Add(LoadLog::Entry(MakeTime(12),
                          LoadLog::Event(LoadLog::TYPE_CANCELLED,
                                         LoadLog::PHASE_NONE)));

  log->Add(LoadLog::Entry(MakeTime(131),
                          LoadLog::Event(LoadLog::TYPE_HOST_RESOLVER_IMPL,
                                         LoadLog::PHASE_END)));

  EXPECT_EQ(
    "t=  1: +HOST_RESOLVER_IMPL                    [dt=130]\n"
    "t=  5:    HOST_RESOLVER_IMPL_OBSERVER_ONSTART [dt=  3]\n"
    "t= 12:    CANCELLED\n"
    "t=131: -HOST_RESOLVER_IMPL",
    LoadLogUtil::PrettyPrintAsEventTree(log));
}

TEST(LoadLogUtilTest, Basic2) {
  scoped_refptr<LoadLog> log(new LoadLog(10));

  log->Add(LoadLog::Entry(MakeTime(1),
                          LoadLog::Event(LoadLog::TYPE_HOST_RESOLVER_IMPL,
                                         LoadLog::PHASE_BEGIN)));

  log->Add(LoadLog::Entry(MakeTime(12), "Sup foo"));
  log->Add(LoadLog::Entry(MakeTime(12), ERR_UNEXPECTED));
  log->Add(LoadLog::Entry(MakeTime(14), "Multiline\nString"));

  log->Add(LoadLog::Entry(MakeTime(131),
                          LoadLog::Event(LoadLog::TYPE_HOST_RESOLVER_IMPL,
                                         LoadLog::PHASE_END)));

  EXPECT_EQ(
    "t=  1: +HOST_RESOLVER_IMPL   [dt=130]\n"
    "t= 12:    \"Sup foo\"\n"
    "t= 12:    error code: -9 (net::ERR_UNEXPECTED)\n"
    "t= 14:    \"Multiline\n"
    "String\"\n"
    "t=131: -HOST_RESOLVER_IMPL",
    LoadLogUtil::PrettyPrintAsEventTree(log));
}

TEST(LoadLogUtilTest, UnmatchedOpen) {
  scoped_refptr<LoadLog> log(new LoadLog(10));

  log->Add(LoadLog::Entry(MakeTime(3),
                          LoadLog::Event(LoadLog::TYPE_HOST_RESOLVER_IMPL,
                                         LoadLog::PHASE_BEGIN)));
  // Note that there is no matching call to PHASE_END for all of the following.
  log->Add(
      LoadLog::Entry(
          MakeTime(6),
          LoadLog::Event(LoadLog::TYPE_HOST_RESOLVER_IMPL_OBSERVER_ONSTART,
                         LoadLog::PHASE_BEGIN)));
  log->Add(
      LoadLog::Entry(
          MakeTime(7),
          LoadLog::Event(LoadLog::TYPE_HOST_RESOLVER_IMPL_OBSERVER_ONSTART,
                         LoadLog::PHASE_BEGIN)));
  log->Add(
      LoadLog::Entry(
          MakeTime(8),
          LoadLog::Event(LoadLog::TYPE_HOST_RESOLVER_IMPL_OBSERVER_ONSTART,
                         LoadLog::PHASE_BEGIN)));
  log->Add(LoadLog::Entry(MakeTime(10),
                          LoadLog::Event(LoadLog::TYPE_CANCELLED,
                                         LoadLog::PHASE_NONE)));
  log->Add(LoadLog::Entry(MakeTime(16),
                          LoadLog::Event(LoadLog::TYPE_HOST_RESOLVER_IMPL,
                                         LoadLog::PHASE_END)));

  EXPECT_EQ(
    "t= 3: +HOST_RESOLVER_IMPL                          [dt=13]\n"
    "t= 6:   +HOST_RESOLVER_IMPL_OBSERVER_ONSTART       [dt=10]\n"
    "t= 7:     +HOST_RESOLVER_IMPL_OBSERVER_ONSTART     [dt= 9]\n"
    "t= 8:       +HOST_RESOLVER_IMPL_OBSERVER_ONSTART   [dt= 8]\n"
    "t=10:          CANCELLED\n"
    "t=16: -HOST_RESOLVER_IMPL",
    LoadLogUtil::PrettyPrintAsEventTree(log));
}

TEST(LoadLogUtilTest, DisplayOfTruncated) {
  size_t kMaxNumEntries = 5;
  scoped_refptr<LoadLog> log(new LoadLog(kMaxNumEntries));

  // Add a total of 10 events. This means that 5 will be truncated.
  log->Add(LoadLog::Entry(MakeTime(0),
                          LoadLog::Event(LoadLog::TYPE_TCP_CONNECT,
                                         LoadLog::PHASE_BEGIN)));
  for (size_t i = 1; i < 8; ++i) {
    log->Add(LoadLog::Entry(MakeTime(i),
                            LoadLog::Event(LoadLog::TYPE_CANCELLED,
                                           LoadLog::PHASE_NONE)));
  }
  log->Add(LoadLog::Entry(MakeTime(9),
                          LoadLog::Event(LoadLog::TYPE_TCP_CONNECT,
                                         LoadLog::PHASE_END)));

  EXPECT_EQ(
    "t=0: +TCP_CONNECT   [dt=9]\n"
    "t=1:    CANCELLED\n"
    "t=2:    CANCELLED\n"
    "t=3:    CANCELLED\n"
    " ... Truncated 4 entries ...\n"
    "t=9: -TCP_CONNECT",
    LoadLogUtil::PrettyPrintAsEventTree(log));
}

}  // namespace
}  // namespace net
