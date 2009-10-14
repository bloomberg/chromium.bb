// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/visit_log.h"
#include "testing/gtest/include/gtest/gtest.h"

// A simple test that makes sure events are recorded correctly in circular
// buffer.
TEST(VisitLog, SimpleRecording) {
  history::VisitLog vlog;

  unsigned char* vlog_buffer = vlog.events();
  int vlog_buffer_size = vlog.events_buffer_size();

  ASSERT_EQ(history::VisitLog::NO_OP, vlog_buffer[0]);
  ASSERT_EQ(history::VisitLog::NO_OP, vlog_buffer[1]);
  ASSERT_EQ(history::VisitLog::NO_OP, vlog_buffer[2]);
  ASSERT_EQ(history::VisitLog::NO_OP, vlog_buffer[vlog_buffer_size-1]);

  for (int i = 0; i < vlog_buffer_size; i++)
    vlog.AddEvent(history::VisitLog::ADD_VISIT);

  ASSERT_EQ(history::VisitLog::ADD_VISIT, vlog_buffer[0]);
  ASSERT_EQ(history::VisitLog::ADD_VISIT, vlog_buffer[1]);
  ASSERT_EQ(history::VisitLog::ADD_VISIT, vlog_buffer[2]);
  ASSERT_EQ(history::VisitLog::ADD_VISIT, vlog_buffer[vlog_buffer_size-1]);
  ASSERT_EQ(vlog_buffer_size, vlog.num_add());
  ASSERT_EQ(0, vlog.num_delete());
  ASSERT_EQ(0, vlog.num_update());
  ASSERT_EQ(0, vlog.num_select());

  vlog.AddEvent(history::VisitLog::DELETE_VISIT);
  vlog.AddEvent(history::VisitLog::UPDATE_VISIT);
  vlog.AddEvent(history::VisitLog::SELECT_VISIT);

  ASSERT_EQ(history::VisitLog::DELETE_VISIT, vlog_buffer[0]);
  ASSERT_EQ(history::VisitLog::UPDATE_VISIT, vlog_buffer[1]);
  ASSERT_EQ(history::VisitLog::SELECT_VISIT, vlog_buffer[2]);
  ASSERT_EQ(history::VisitLog::ADD_VISIT, vlog_buffer[vlog_buffer_size-1]);
  ASSERT_EQ(vlog_buffer_size, vlog.num_add());
  ASSERT_EQ(1, vlog.num_delete());
  ASSERT_EQ(1, vlog.num_update());
  ASSERT_EQ(1, vlog.num_select());
}
