// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/breakpad_field_trial_win.h"

#include "base/lazy_instance.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "breakpad/src/client/windows/common/ipc_protocol.h"
#include "chrome/app/breakpad_win.h"
#include "chrome/common/child_process_logging.h"

// Sets the breakpad experiment chunks for crash reporting. |chunks| is an
// array of C strings of size |chunk_size| containing the values to set, where
// each entry may contain multiple experiment tuples, with the total number of
// experiments indicated by |experiments_chunks|.
// Note that this is suffixed with "3" due to parameter changes that were made
// to the predecessor functions. If the signature changes again, use a new name.
extern "C" void __declspec(dllexport) __cdecl SetExperimentList3(
      const wchar_t** chunks,
      size_t chunks_size,
      size_t experiments_count) {
  // Make sure the offset was initialized before storing the data.
  if (breakpad_win::g_experiment_chunks_offset == 0)
    return;

  // Store up to |kMaxReportedVariationChunks| chunks.
  const size_t number_of_chunks_to_report =
      std::min(chunks_size, kMaxReportedVariationChunks);
  for (size_t i = 0; i < number_of_chunks_to_report; ++i) {
    const size_t entry_index = breakpad_win::g_experiment_chunks_offset + i;
    (*breakpad_win::g_custom_entries)[entry_index].set_value(chunks[i]);
  }

  // Make note of the total number of experiments, which may be greater than
  // what was able to fit in |kMaxReportedVariationChunks|. This is useful when
  // correlating stability with the number of experiments running
  // simultaneously.
  base::wcslcpy(
      (*breakpad_win::g_custom_entries)[
          breakpad_win::g_num_of_experiments_offset].value,
      base::StringPrintf(
          L"%d", static_cast<int>(experiments_count)).c_str(),
      google_breakpad::CustomInfoEntry::kValueMaxLength);
}

namespace testing {

void SetExperimentChunks(const std::vector<string16>& chunks,
                         size_t experiments_count) {
  std::vector<const wchar_t*> cstrings;
  StringVectorToCStringVector(chunks, &cstrings);
  ::SetExperimentList3(&cstrings[0], cstrings.size(), experiments_count);
}

}  // namespace testing
