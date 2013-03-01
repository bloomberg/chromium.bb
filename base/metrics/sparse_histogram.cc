// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/sparse_histogram.h"

#include "base/metrics/sample_map.h"
#include "base/metrics/statistics_recorder.h"
#include "base/pickle.h"
#include "base/synchronization/lock.h"

using std::map;
using std::string;

namespace base {

typedef HistogramBase::Count Count;
typedef HistogramBase::Sample Sample;

// static
HistogramBase* SparseHistogram::FactoryGet(const string& name, int32 flags) {
  HistogramBase* histogram = StatisticsRecorder::FindHistogram(name);

  if (!histogram) {
    // To avoid racy destruction at shutdown, the following will be leaked.
    HistogramBase* tentative_histogram = new SparseHistogram(name);
    tentative_histogram->SetFlags(flags);
    histogram =
        StatisticsRecorder::RegisterOrDeleteDuplicate(tentative_histogram);
  }
  DCHECK_EQ(SPARSE_HISTOGRAM, histogram->GetHistogramType());
  return histogram;
}

SparseHistogram::~SparseHistogram() {}

HistogramType SparseHistogram::GetHistogramType() const {
  return SPARSE_HISTOGRAM;
}

bool SparseHistogram::HasConstructionArguments(Sample minimum,
                                               Sample maximum,
                                               size_t bucket_count) const {
  // SparseHistogram never has min/max/bucket_count limit.
  return false;
}

void SparseHistogram::Add(Sample value) {
  base::AutoLock auto_lock(lock_);
  samples_.Accumulate(value, 1);
}

scoped_ptr<HistogramSamples> SparseHistogram::SnapshotSamples() const {
  scoped_ptr<SampleMap> snapshot(new SampleMap());

  base::AutoLock auto_lock(lock_);
  snapshot->Add(samples_);
  return snapshot.PassAs<HistogramSamples>();
}

void SparseHistogram::AddSamples(const HistogramSamples& samples) {
  base::AutoLock auto_lock(lock_);
  samples_.Add(samples);
}

bool SparseHistogram::AddSamplesFromPickle(PickleIterator* iter) {
  base::AutoLock auto_lock(lock_);
  return samples_.AddFromPickle(iter);
}

void SparseHistogram::WriteHTMLGraph(string* output) const {
  // TODO(kaiwang): Implement.
}

void SparseHistogram::WriteAscii(string* output) const {
  // TODO(kaiwang): Implement.
}

bool SparseHistogram::SerializeInfoImpl(Pickle* pickle) const {
  return pickle->WriteString(histogram_name()) && pickle->WriteInt(flags());
}

SparseHistogram::SparseHistogram(const string& name)
    : HistogramBase(name) {}

HistogramBase* SparseHistogram::DeserializeInfoImpl(PickleIterator* iter) {
  string histogram_name;
  int flags;
  if (!iter->ReadString(&histogram_name) || !iter->ReadInt(&flags)) {
    DLOG(ERROR) << "Pickle error decoding Histogram: " << histogram_name;
    return NULL;
  }

  DCHECK(flags & HistogramBase::kIPCSerializationSourceFlag);
  flags &= ~HistogramBase::kIPCSerializationSourceFlag;

  return SparseHistogram::FactoryGet(histogram_name, flags);
}

void SparseHistogram::GetParameters(DictionaryValue* params) const {
  // TODO(kaiwang): Implement. (See HistogramBase::WriteJSON.)
}

void SparseHistogram::GetCountAndBucketData(Count* count,
                                            ListValue* buckets) const {
  // TODO(kaiwang): Implement. (See HistogramBase::WriteJSON.)
}

}  // namespace base
