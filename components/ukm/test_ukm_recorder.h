// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_TEST_UKM_RECORDER_H_
#define COMPONENTS_UKM_TEST_UKM_RECORDER_H_

#include <stddef.h>
#include <memory>

#include "base/macros.h"
#include "components/ukm/ukm_recorder_impl.h"

namespace ukm {

// Wraps an UkmRecorder with additional accessors used for testing.
class TestUkmRecorder : public UkmRecorderImpl {
 public:
  TestUkmRecorder();
  ~TestUkmRecorder() override;

  size_t sources_count() const { return sources().size(); }
  const std::map<ukm::SourceId, std::unique_ptr<UkmSource>>& GetSources()
      const {
    return sources();
  }
  const UkmSource* GetSourceForUrl(const char* url) const;
  const UkmSource* GetSourceForSourceId(ukm::SourceId source_id) const;

  size_t entries_count() const { return entries().size(); }
  const mojom::UkmEntry* GetEntry(size_t entry_num) const;
  const mojom::UkmEntry* GetEntryForEntryName(const char* entry_name) const;

  static const mojom::UkmMetric* FindMetric(const mojom::UkmEntry* entry,
                                            const char* metric_name);

 private:
  DISALLOW_COPY_AND_ASSIGN(TestUkmRecorder);
};

}  // namespace ukm

#endif  // COMPONENTS_UKM_TEST_UKM_RECORDER_H_
