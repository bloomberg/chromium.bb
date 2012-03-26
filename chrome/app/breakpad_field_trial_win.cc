// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/breakpad_field_trial_win.h"

#include "base/lazy_instance.h"
#include "base/metrics/field_trial.h"
#include "base/string_util.h"
#include "base/string16.h"
#include "base/stringprintf.h"
#include "breakpad/src/client/windows/common/ipc_protocol.h"
#include "chrome/app/breakpad_win.h"
#include "chrome/common/child_process_logging.h"

namespace {

// Use this wrapper to be able to pass in the EmptyString to the constructor.
// We could also use a traits, but we would still need a class, so...
class FieldTrialListWrapper : base::FieldTrialList {
 public:
  FieldTrialListWrapper() : base::FieldTrialList(EmptyString()) {}
};

base::LazyInstance<FieldTrialListWrapper> g_field_trial_list =
    LAZY_INSTANCE_INITIALIZER;

}

extern "C" void __declspec(dllexport) __cdecl InitExperimentList(
    const std::string& state) {
  // Make sure the global instance is created.
  g_field_trial_list.Pointer();
  base::FieldTrialList::CreateTrialsInChildProcess(state);
  UpdateExperiments();
}

extern "C" void __declspec(dllexport) __cdecl AddFieldTrialGroup(
    const std::string& field_trial_name, const std::string& group_name) {
  base::FieldTrialList::CreateFieldTrial(field_trial_name, group_name);
  // TODO(mad): Find a way to just add the |field_trial_name| and |group_name|
  // instead of starting over each time.
  UpdateExperiments();
}

void UpdateExperiments() {
  // Make sure we were initialized before we start writing data
  if (breakpad_win::g_experiment_chunks_offset == 0)
    return;

  std::vector<base::FieldTrial::NameGroupId> name_group_ids;
  base::FieldTrialList::GetFieldTrialNameGroupIds(&name_group_ids);

  std::vector<string16> experiment_strings(name_group_ids.size());
  for (size_t i = 0; i < name_group_ids.size(); ++i) {
    experiment_strings[i] = base::StringPrintf(
        L"%x-%x", name_group_ids[i].name, name_group_ids[i].group);
  }

  size_t num_chunks = 0;
  size_t current_experiment = 0;
  string16 current_chunk(google_breakpad::CustomInfoEntry::kValueMaxLength, 0);
  while (current_experiment < experiment_strings.size() &&
         num_chunks < kMaxReportedExperimentChunks) {
    // Check if we have enough room to add another experiment to the current
    // chunk string. If not, we commit the current chunk string and start over.
    if (current_chunk.size() + experiment_strings[current_experiment].size() >
        google_breakpad::CustomInfoEntry::kValueMaxLength) {
      base::wcslcpy(
          (*breakpad_win::g_custom_entries)[
              breakpad_win::g_experiment_chunks_offset + num_chunks].value,
          current_chunk.c_str(),
          current_chunk.size() + 1);  // This must include the NULL termination.
      ++num_chunks;
      current_chunk = experiment_strings[current_experiment];
    } else {
      if (!current_chunk.empty())
        current_chunk += L",";
      current_chunk += experiment_strings[current_experiment];
    }
    ++current_experiment;
  }

  // Commit the last chunk that didn't get big enough yet.
  if (!current_chunk.empty() && num_chunks < kMaxReportedExperimentChunks) {
    base::wcslcpy(
        (*breakpad_win::g_custom_entries)[
            breakpad_win::g_experiment_chunks_offset + num_chunks].value,
        current_chunk.c_str(),
        current_chunk.size() + 1);  // This must include the NULL termination.
  }

  // Make note of the total number of experiments,
  // even if it's > kMaxReportedExperimentChunks. This is useful when
  // correlating stability with the number of experiments running
  // simultaneously.
  base::wcslcpy(
      (*breakpad_win::g_custom_entries)[
          breakpad_win::g_num_of_experiments_offset].value,
      base::StringPrintf(L"%d", name_group_ids.size()).c_str(),
      google_breakpad::CustomInfoEntry::kValueMaxLength);
}
