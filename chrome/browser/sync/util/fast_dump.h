// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_UTIL_FAST_DUMP_H_
#define CHROME_BROWSER_SYNC_UTIL_FAST_DUMP_H_
#pragma once

#include <ostream>
#include <streambuf>
#include <string>

#include "base/string_number_conversions.h"

using std::ostream;
using std::streambuf;
using std::string;

// This seems totally gratuitous, and it would be if std::ostream was fast, but
// std::ostream is slow. It's slow because it creates a ostream::sentry (mutex)
// for every little << operator. When you want to dump a whole lot of stuff
// under a single mutex lock, use this FastDump class.
namespace browser_sync {
class FastDump {
 public:
  explicit FastDump(ostream* outs) : sentry_(*outs), out_(outs->rdbuf()) {
  }
  ostream::sentry sentry_;
  streambuf* const out_;
};

inline FastDump& operator << (FastDump& dump, int64 n) {
  string numbuf(base::Int64ToString(n));
  const char* number = numbuf.c_str();
  dump.out_->sputn(number, numbuf.length());
  return dump;
}

inline FastDump& operator << (FastDump& dump, int32 n) {
  string numbuf(base::IntToString(n));
  const char* number = numbuf.c_str();
  dump.out_->sputn(number, numbuf.length());
  return dump;
}

inline FastDump& operator << (FastDump& dump, const char* s) {
  dump.out_->sputn(s, strlen(s));
  return dump;
}

inline FastDump& operator << (FastDump& dump, const string& s) {
  dump.out_->sputn(s.data(), s.size());
  return dump;
}

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_UTIL_FAST_DUMP_H_
