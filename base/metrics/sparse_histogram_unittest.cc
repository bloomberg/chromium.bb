// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/sample_map.h"
#include "base/metrics/sparse_histogram.h"
#include "base/pickle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class SparseHistogramTest : public testing::Test {
 protected:
  scoped_ptr<SparseHistogram> NewSparseHistogram(const std::string& name) {
    return scoped_ptr<SparseHistogram>(new SparseHistogram(name));
  }
};

TEST_F(SparseHistogramTest, BasicTest) {
  scoped_ptr<SparseHistogram> histogram(NewSparseHistogram("Sparse"));
  scoped_ptr<HistogramSamples> snapshot(histogram->SnapshotSamples());
  EXPECT_EQ(0, snapshot->TotalCount());
  EXPECT_EQ(0, snapshot->sum());

  histogram->Add(100);
  scoped_ptr<HistogramSamples> snapshot1(histogram->SnapshotSamples());
  EXPECT_EQ(1, snapshot1->TotalCount());
  EXPECT_EQ(1, snapshot1->GetCount(100));

  histogram->Add(100);
  histogram->Add(101);
  scoped_ptr<HistogramSamples> snapshot2(histogram->SnapshotSamples());
  EXPECT_EQ(3, snapshot2->TotalCount());
  EXPECT_EQ(2, snapshot2->GetCount(100));
  EXPECT_EQ(1, snapshot2->GetCount(101));
}

TEST_F(SparseHistogramTest, Serialize) {
  scoped_ptr<SparseHistogram> histogram(NewSparseHistogram("Sparse"));
  histogram->SetFlags(HistogramBase::kIPCSerializationSourceFlag);

  Pickle pickle;
  histogram->SerializeInfo(&pickle);

  PickleIterator iter(pickle);

  int type;
  EXPECT_TRUE(iter.ReadInt(&type));
  EXPECT_EQ(SPARSE_HISTOGRAM, type);

  std::string name;
  EXPECT_TRUE(iter.ReadString(&name));
  EXPECT_EQ("Sparse", name);

  int flag;
  EXPECT_TRUE(iter.ReadInt(&flag));
  EXPECT_EQ(HistogramBase::kIPCSerializationSourceFlag, flag);

  // No more data in the pickle.
  EXPECT_FALSE(iter.SkipBytes(1));
}

}  // namespace base
