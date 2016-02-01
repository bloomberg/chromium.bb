// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_persistence.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/synchronization/lock.h"

namespace base {

namespace {

// Enumerate possible creation results for reporting.
enum CreateHistogramResultType {
  // Everything was fine.
  CREATE_HISTOGRAM_SUCCESS = 0,

  // Pointer to metadata was not valid.
  CREATE_HISTOGRAM_INVALID_METADATA_POINTER,

  // Histogram metadata was not valid.
  CREATE_HISTOGRAM_INVALID_METADATA,

  // Ranges information was not valid.
  CREATE_HISTOGRAM_INVALID_RANGES_ARRAY,

  // Counts information was not valid.
  CREATE_HISTOGRAM_INVALID_COUNTS_ARRAY,

  // Could not allocate histogram memory due to corruption.
  CREATE_HISTOGRAM_ALLOCATOR_CORRUPT,

  // Could not allocate histogram memory due to lack of space.
  CREATE_HISTOGRAM_ALLOCATOR_FULL,

  // Could not allocate histogram memory due to unknown error.
  CREATE_HISTOGRAM_ALLOCATOR_ERROR,

  // Always keep this at the end.
  CREATE_HISTOGRAM_MAX
};

// Type identifiers used when storing in persistent memory so they can be
// identified during extraction; the first 4 bytes of the SHA1 of the name
// is used as a unique integer. A "version number" is added to the base
// so that, if the structure of that object changes, stored older versions
// will be safely ignored.
enum : uint32_t {
  kTypeIdHistogram   = 0xF1645910 + 1,  // SHA1(Histogram) v1
  kTypeIdRangesArray = 0xBCEA225A + 1,  // SHA1(RangesArray) v1
  kTypeIdCountsArray = 0x53215530 + 1,  // SHA1(CountsArray) v1
};

// This data must be held in persistent memory in order for processes to
// locate and use histograms created elsewhere.
struct PersistentHistogramData {
  int histogram_type;
  int flags;
  int minimum;
  int maximum;
  size_t bucket_count;
  PersistentMemoryAllocator::Reference ranges_ref;
  uint32_t ranges_checksum;
  PersistentMemoryAllocator::Reference counts_ref;
  HistogramSamples::Metadata samples_metadata;

  // Space for the histogram name will be added during the actual allocation
  // request. This must be the last field of the structure. A zero-size array
  // or a "flexible" array would be preferred but is not (yet) valid C++.
  char name[1];
};

// The object held here will obviously not be destructed at process exit
// but that's okay since PersistentMemoryAllocator objects are explicitly
// forbidden from doing anything essential at exit anyway due to the fact
// that they depend on data managed elsewhere and which could be destructed
// first.
PersistentMemoryAllocator* g_allocator = nullptr;

// Take an array of range boundaries and create a proper BucketRanges object
// which is returned to the caller. A return of nullptr indicates that the
// passed boundaries are invalid.
BucketRanges* CreateRangesFromData(HistogramBase::Sample* ranges_data,
                                   uint32_t ranges_checksum,
                                   size_t count) {
  scoped_ptr<BucketRanges> ranges(new BucketRanges(count));
  DCHECK_EQ(count, ranges->size());
  for (size_t i = 0; i < count; ++i) {
    if (i > 0 && ranges_data[i] <= ranges_data[i - 1])
      return nullptr;
    ranges->set_range(i, ranges_data[i]);
  }

  ranges->ResetChecksum();
  if (ranges->checksum() != ranges_checksum)
    return nullptr;

  return ranges.release();
}

}  // namespace

const Feature kPersistentHistogramsFeature{
  "PersistentHistograms", FEATURE_DISABLED_BY_DEFAULT
};

// Get the histogram in which create results are stored. This is copied almost
// exactly from the STATIC_HISTOGRAM_POINTER_BLOCK macro but with added code
// to prevent recursion (a likely occurance because the creation of a new
// histogram can end up calling this.)
HistogramBase* GetCreateHistogramResultHistogram() {
  static base::subtle::AtomicWord atomic_histogram_pointer = 0;
  HistogramBase* histogram_pointer(
      reinterpret_cast<HistogramBase*>(
          base::subtle::Acquire_Load(&atomic_histogram_pointer)));
  if (!histogram_pointer) {
    // It's possible for multiple threads to make it here in parallel but
    // they'll always return the same result as there is a mutex in the Get.
    // The purpose of the "initialized" variable is just to ensure that
    // the same thread doesn't recurse which is also why it doesn't have
    // to be atomic.
    static bool initialized = false;
    if (!initialized) {
      initialized = true;
      histogram_pointer = LinearHistogram::FactoryGet(
          "UMA.CreatePersistentHistogram.Result",
          1, CREATE_HISTOGRAM_MAX, CREATE_HISTOGRAM_MAX + 1,
          HistogramBase::kUmaTargetedHistogramFlag);
      base::subtle::Release_Store(
          &atomic_histogram_pointer,
          reinterpret_cast<base::subtle::AtomicWord>(histogram_pointer));
    }
  }
  return histogram_pointer;
}

// Record the result of a histogram creation.
void RecordCreateHistogramResult(CreateHistogramResultType result) {
  HistogramBase* result_histogram = GetCreateHistogramResultHistogram();
  if (result_histogram)
    result_histogram->Add(result);
}

void SetPersistentHistogramMemoryAllocator(
    PersistentMemoryAllocator* allocator) {
  // Releasing or changing an allocator is extremely dangerous because it
  // likely has histograms stored within it. If the backing memory is also
  // also released, future accesses to those histograms will seg-fault.
  // It's not a fatal CHECK() because tests do this knowing that all
  // such persistent histograms have already been forgotten.
  if (g_allocator) {
    LOG(WARNING) << "Active PersistentMemoryAllocator has been released."
                 << " Some existing histogram pointers may be invalid.";
    delete g_allocator;
  }
  g_allocator = allocator;
}

PersistentMemoryAllocator* GetPersistentHistogramMemoryAllocator() {
  return g_allocator;
}

PersistentMemoryAllocator* ReleasePersistentHistogramMemoryAllocator() {
  PersistentMemoryAllocator* allocator = g_allocator;
  g_allocator = nullptr;
  return allocator;
};

HistogramBase* CreatePersistentHistogram(
    PersistentMemoryAllocator* allocator,
    PersistentHistogramData* histogram_data_ptr) {
  if (!histogram_data_ptr) {
    RecordCreateHistogramResult(CREATE_HISTOGRAM_INVALID_METADATA_POINTER);
    NOTREACHED();
    return nullptr;
  }

  // Copy the histogram_data to local storage because anything in persistent
  // memory cannot be trusted as it could be changed at any moment by a
  // malicious actor that shares access. The contents of histogram_data are
  // validated below; the local copy is to ensure that the contents cannot
  // be externally changed between validation and use.
  PersistentHistogramData histogram_data = *histogram_data_ptr;

  HistogramBase::Sample* ranges_data =
      allocator->GetAsObject<HistogramBase::Sample>(histogram_data.ranges_ref,
                                                    kTypeIdRangesArray);
  if (!ranges_data || histogram_data.bucket_count < 2 ||
      histogram_data.bucket_count + 1 >
          std::numeric_limits<size_t>::max() / sizeof(HistogramBase::Sample) ||
      allocator->GetAllocSize(histogram_data.ranges_ref) <
          (histogram_data.bucket_count + 1) * sizeof(HistogramBase::Sample)) {
    RecordCreateHistogramResult(CREATE_HISTOGRAM_INVALID_RANGES_ARRAY);
    NOTREACHED();
    return nullptr;
  }
  // To avoid racy destruction at shutdown, the following will be leaked.
  const BucketRanges* ranges = CreateRangesFromData(
      ranges_data,
      histogram_data.ranges_checksum,
      histogram_data.bucket_count + 1);
  if (!ranges) {
    RecordCreateHistogramResult(CREATE_HISTOGRAM_INVALID_RANGES_ARRAY);
    NOTREACHED();
    return nullptr;
  }
  ranges = StatisticsRecorder::RegisterOrDeleteDuplicateRanges(ranges);

  HistogramBase::AtomicCount* counts_data =
      allocator->GetAsObject<HistogramBase::AtomicCount>(
          histogram_data.counts_ref, kTypeIdCountsArray);
  if (!counts_data ||
      allocator->GetAllocSize(histogram_data.counts_ref) <
          histogram_data.bucket_count * sizeof(HistogramBase::AtomicCount)) {
    RecordCreateHistogramResult(CREATE_HISTOGRAM_INVALID_COUNTS_ARRAY);
    NOTREACHED();
    return nullptr;
  }

  std::string name(histogram_data_ptr->name);
  HistogramBase* histogram = nullptr;
  switch (histogram_data.histogram_type) {
    case HISTOGRAM:
      histogram = Histogram::PersistentGet(
          name,
          histogram_data.minimum,
          histogram_data.maximum,
          ranges,
          counts_data,
          histogram_data.bucket_count,
          &histogram_data_ptr->samples_metadata);
      break;
    case LINEAR_HISTOGRAM:
      histogram = LinearHistogram::PersistentGet(
          name,
          histogram_data.minimum,
          histogram_data.maximum,
          ranges,
          counts_data,
          histogram_data.bucket_count,
          &histogram_data_ptr->samples_metadata);
      break;
    case BOOLEAN_HISTOGRAM:
      histogram = BooleanHistogram::PersistentGet(
          name,
          ranges,
          counts_data,
          &histogram_data_ptr->samples_metadata);
      break;
    case CUSTOM_HISTOGRAM:
      histogram = CustomHistogram::PersistentGet(
          name,
          ranges,
          counts_data,
          histogram_data.bucket_count,
          &histogram_data_ptr->samples_metadata);
      break;
  }

  if (histogram) {
    DCHECK_EQ(histogram_data.histogram_type, histogram->GetHistogramType());
    histogram->SetFlags(histogram_data.flags);
  }

  RecordCreateHistogramResult(CREATE_HISTOGRAM_SUCCESS);
  return histogram;
}

HistogramBase* GetPersistentHistogram(
    PersistentMemoryAllocator* allocator,
    int32_t ref) {
  // Unfortunately, the above "pickle" methods cannot be used as part of the
  // persistance because the deserialization methods always create local
  // count data (these must referenced the persistent counts) and always add
  // it to the local list of known histograms (these may be simple references
  // to histograms in other processes).
  PersistentHistogramData* histogram_data =
      allocator->GetAsObject<PersistentHistogramData>(ref, kTypeIdHistogram);
  size_t length = allocator->GetAllocSize(ref);
  if (!histogram_data ||
      reinterpret_cast<char*>(histogram_data)[length - 1] != '\0') {
    RecordCreateHistogramResult(CREATE_HISTOGRAM_INVALID_METADATA);
    NOTREACHED();
    return nullptr;
  }
  return CreatePersistentHistogram(allocator, histogram_data);
}

HistogramBase* GetNextPersistentHistogram(
    PersistentMemoryAllocator* allocator,
    PersistentMemoryAllocator::Iterator* iter) {
  PersistentMemoryAllocator::Reference ref;
  uint32_t type_id;
  while ((ref = allocator->GetNextIterable(iter, &type_id)) != 0) {
    if (type_id == kTypeIdHistogram)
      return GetPersistentHistogram(allocator, ref);
  }
  return nullptr;
}

void FinalizePersistentHistogram(PersistentMemoryAllocator::Reference ref,
                                 bool registered) {
  // If the created persistent histogram was registered then it needs to
  // be marked as "iterable" in order to be found by other processes.
  if (registered)
    GetPersistentHistogramMemoryAllocator()->MakeIterable(ref);
  // If it wasn't registered then a race condition must have caused
  // two to be created. The allocator does not support releasing the
  // acquired memory so just change the type to be empty.
  else
    GetPersistentHistogramMemoryAllocator()->SetType(ref, 0);
}

HistogramBase* AllocatePersistentHistogram(
    PersistentMemoryAllocator* allocator,
    HistogramType histogram_type,
    const std::string& name,
    int minimum,
    int maximum,
    const BucketRanges* bucket_ranges,
    int32_t flags,
    PersistentMemoryAllocator::Reference* ref_ptr) {
  if (!allocator)
    return nullptr;

  size_t bucket_count = bucket_ranges->bucket_count();
  // An overflow such as this, perhaps as the result of a milicious actor,
  // could lead to writing beyond the allocation boundary and into other
  // memory. Just fail the allocation and let the caller deal with it.
  if (bucket_count > std::numeric_limits<int32_t>::max() /
                     sizeof(HistogramBase::AtomicCount)) {
    NOTREACHED();
    return nullptr;
  }
  size_t counts_bytes = bucket_count * sizeof(HistogramBase::AtomicCount);
  size_t ranges_bytes = (bucket_count + 1) * sizeof(HistogramBase::Sample);
  PersistentMemoryAllocator::Reference ranges_ref =
      allocator->Allocate(ranges_bytes, kTypeIdRangesArray);
  PersistentMemoryAllocator::Reference counts_ref =
      allocator->Allocate(counts_bytes, kTypeIdCountsArray);
  PersistentMemoryAllocator::Reference histogram_ref =
      allocator->Allocate(offsetof(PersistentHistogramData, name) +
                          name.length() + 1, kTypeIdHistogram);
  HistogramBase::Sample* ranges_data =
      allocator->GetAsObject<HistogramBase::Sample>(ranges_ref,
                                                    kTypeIdRangesArray);
  PersistentHistogramData* histogram_data =
      allocator->GetAsObject<PersistentHistogramData>(histogram_ref,
                                                      kTypeIdHistogram);

  // Only continue here if all allocations were successful. If they weren't
  // there is no way to free the space but that's not really a problem since
  // the allocations only fail because the space is full and so any future
  // attempts will also fail.
  if (counts_ref && ranges_data && histogram_data) {
    strcpy(histogram_data->name, name.c_str());
    for (size_t i = 0; i < bucket_ranges->size(); ++i)
      ranges_data[i] = bucket_ranges->range(i);

    histogram_data->histogram_type = histogram_type;
    histogram_data->flags = flags;
    histogram_data->minimum = minimum;
    histogram_data->maximum = maximum;
    histogram_data->bucket_count = bucket_count;
    histogram_data->ranges_ref = ranges_ref;
    histogram_data->ranges_checksum = bucket_ranges->checksum();
    histogram_data->counts_ref = counts_ref;

    // Create the histogram using resources in persistent memory. This ends up
    // resolving the "ref" values stored in histogram_data instad of just
    // using what is already known above but avoids duplicating the switch
    // statement here and serves as a double-check that everything is
    // correct before commiting the new histogram to persistent space.
    HistogramBase* histogram =
        CreatePersistentHistogram(allocator, histogram_data);
    DCHECK(histogram);
    if (ref_ptr != nullptr)
      *ref_ptr = histogram_ref;
    return histogram;
  }

  CreateHistogramResultType result;
  if (allocator->IsCorrupt()) {
    result = CREATE_HISTOGRAM_ALLOCATOR_CORRUPT;
  } else if (allocator->IsFull()) {
    result = CREATE_HISTOGRAM_ALLOCATOR_FULL;
  } else {
    result = CREATE_HISTOGRAM_ALLOCATOR_ERROR;
  }
  RecordCreateHistogramResult(result);

  return nullptr;
}

void ImportPersistentHistograms() {
  // Each call resumes from where it last left off so need persistant iterator.
  // The lock protects against concurrent access to the iterator and is created
  // dynamically so as to not require destruction during program exit.
  static PersistentMemoryAllocator::Iterator iter;
  static base::Lock* lock = new base::Lock();

  if (g_allocator) {
    base::AutoLock auto_lock(*lock);
    if (iter.is_clear())
      g_allocator->CreateIterator(&iter);

    for (;;) {
      HistogramBase* histogram = GetNextPersistentHistogram(g_allocator, &iter);
      if (!histogram)
        break;
      StatisticsRecorder::RegisterOrDeleteDuplicate(histogram);
    }
  }
}

}  // namespace base
