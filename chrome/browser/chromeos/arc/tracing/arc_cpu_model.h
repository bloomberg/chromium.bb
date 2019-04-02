// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_CPU_MODEL_H_
#define CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_CPU_MODEL_H_

#include <map>
#include <string>

#include "base/values.h"
#include "chrome/browser/chromeos/arc/tracing/arc_cpu_event.h"

namespace arc {

// Contains information about CPU activity events and involved threads.
class ArcCpuModel {
 public:
  static constexpr int kUnknownPid = -1;

  struct ThreadInfo {
    ThreadInfo();
    ThreadInfo(int pid, const std::string& name);

    bool operator==(const ThreadInfo& other) const;

    // Process id or |kUnknownPid| if unknown.
    int pid = kUnknownPid;
    // Name of thread of process in case thread is main thread of the process.
    std::string name;
  };

  using ThreadMap = std::map<int, ThreadInfo>;

  ArcCpuModel();
  ~ArcCpuModel();

  void Reset();

  void CopyFrom(const ArcCpuModel& other);
  base::DictionaryValue Serialize() const;
  bool Load(const base::Value* root);

  bool operator==(const ArcCpuModel& other) const;

  ThreadMap& thread_map() { return thread_map_; }
  const ThreadMap& thread_map() const { return thread_map_; }

  AllCpuEvents& all_cpu_events() { return all_cpu_events_; }
  const AllCpuEvents& all_cpu_events() const { return all_cpu_events_; }

 private:
  ThreadMap thread_map_;
  AllCpuEvents all_cpu_events_;

  DISALLOW_COPY_AND_ASSIGN(ArcCpuModel);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_CPU_MODEL_H_
