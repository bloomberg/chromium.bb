// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/statistics_recorder.h"

#include <memory>

#include "base/at_exit.h"
#include "base/debug/leak_annotations.h"
#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_snapshot_manager.h"
#include "base/metrics/metrics_hashes.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/metrics/record_histogram_checker.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"

namespace {

// Initialize histogram statistics gathering system.
base::LazyInstance<base::StatisticsRecorder>::Leaky g_statistics_recorder_ =
    LAZY_INSTANCE_INITIALIZER;

bool HistogramNameLesser(const base::HistogramBase* a,
                         const base::HistogramBase* b) {
  return strcmp(a->histogram_name(), b->histogram_name()) < 0;
}

}  // namespace

namespace base {

size_t StatisticsRecorder::BucketRangesHash::operator()(
    const BucketRanges* const a) const {
  return a->checksum();
}

bool StatisticsRecorder::BucketRangesEqual::operator()(
    const BucketRanges* const a,
    const BucketRanges* const b) const {
  return a->Equals(b);
}

StatisticsRecorder::~StatisticsRecorder() {
  DCHECK(histograms_);
  DCHECK(ranges_);

  // Clean out what this object created and then restore what existed before.
  Reset();
  base::AutoLock auto_lock(lock_.Get());
  histograms_ = existing_histograms_.release();
  callbacks_ = existing_callbacks_.release();
  ranges_ = existing_ranges_.release();
  providers_ = existing_providers_.release();
  record_checker_ = existing_record_checker_.release();
}

// static
void StatisticsRecorder::Initialize() {
  // Tests sometimes create local StatisticsRecorders in order to provide a
  // contained environment of histograms that can be later discarded. If a
  // true global instance gets created in this environment then it will
  // eventually get disconnected when the local instance destructs and
  // restores the previous state, resulting in no StatisticsRecorder at all.
  // The global lazy instance, however, will remain valid thus ensuring that
  // another never gets installed via this method. If a |histograms_| map
  // exists then assume the StatisticsRecorder is already "initialized".
  if (histograms_)
    return;

  // Ensure that an instance of the StatisticsRecorder object is created.
  g_statistics_recorder_.Get();
}

// static
bool StatisticsRecorder::IsActive() {
  base::AutoLock auto_lock(lock_.Get());
  return histograms_ != nullptr;
}

// static
void StatisticsRecorder::RegisterHistogramProvider(
    const WeakPtr<HistogramProvider>& provider) {
  providers_->push_back(provider);
}

// static
HistogramBase* StatisticsRecorder::RegisterOrDeleteDuplicate(
    HistogramBase* histogram) {
  // Declared before auto_lock to ensure correct destruction order.
  std::unique_ptr<HistogramBase> histogram_deleter;
  base::AutoLock auto_lock(lock_.Get());

  if (!histograms_) {
    // As per crbug.com/79322 the histograms are intentionally leaked, so we
    // need to annotate them. Because ANNOTATE_LEAKING_OBJECT_PTR may be used
    // only once for an object, the duplicates should not be annotated.
    // Callers are responsible for not calling RegisterOrDeleteDuplicate(ptr)
    // twice |if (!histograms_)|.
    ANNOTATE_LEAKING_OBJECT_PTR(histogram);  // see crbug.com/79322
    return histogram;
  }

  const char* const name = histogram->histogram_name();
  HistogramBase*& registered = (*histograms_)[name];

  if (!registered) {
    // |name| is guaranteed to never change or be deallocated so long
    // as the histogram is alive (which is forever).
    registered = histogram;
    ANNOTATE_LEAKING_OBJECT_PTR(histogram);  // see crbug.com/79322
    // If there are callbacks for this histogram, we set the kCallbackExists
    // flag.
    const auto callback_iterator = callbacks_->find(name);
    if (callback_iterator != callbacks_->end()) {
      if (!callback_iterator->second.is_null())
        histogram->SetFlags(HistogramBase::kCallbackExists);
      else
        histogram->ClearFlags(HistogramBase::kCallbackExists);
    }
    return histogram;
  }

  if (histogram == registered) {
    // The histogram was registered before.
    return histogram;
  }

  // We already have one histogram with this name.
  histogram_deleter.reset(histogram);
  return registered;
}

// static
const BucketRanges* StatisticsRecorder::RegisterOrDeleteDuplicateRanges(
    const BucketRanges* ranges) {
  DCHECK(ranges->HasValidChecksum());

  // Declared before auto_lock to ensure correct destruction order.
  std::unique_ptr<const BucketRanges> ranges_deleter;
  base::AutoLock auto_lock(lock_.Get());

  if (!ranges_) {
    ANNOTATE_LEAKING_OBJECT_PTR(ranges);
    return ranges;
  }

  const BucketRanges* const registered = *ranges_->insert(ranges).first;
  if (registered == ranges) {
    ANNOTATE_LEAKING_OBJECT_PTR(ranges);
  } else {
    ranges_deleter.reset(ranges);
  }

  return registered;
}

// static
void StatisticsRecorder::WriteHTMLGraph(const std::string& query,
                                        std::string* output) {
  if (!IsActive())
    return;

  Histograms snapshot;
  GetSnapshot(query, &snapshot);
  std::sort(snapshot.begin(), snapshot.end(), &HistogramNameLesser);
  for (const HistogramBase* histogram : snapshot) {
    histogram->WriteHTMLGraph(output);
    *output += "<br><hr><br>";
  }
}

// static
void StatisticsRecorder::WriteGraph(const std::string& query,
                                    std::string* output) {
  if (!IsActive())
    return;
  if (query.length())
    StringAppendF(output, "Collections of histograms for %s\n", query.c_str());
  else
    output->append("Collections of all histograms\n");

  Histograms snapshot;
  GetSnapshot(query, &snapshot);
  std::sort(snapshot.begin(), snapshot.end(), &HistogramNameLesser);
  for (const HistogramBase* histogram : snapshot) {
    histogram->WriteAscii(output);
    output->append("\n");
  }
}

// static
std::string StatisticsRecorder::ToJSON(JSONVerbosityLevel verbosity_level) {
  if (!IsActive())
    return std::string();

  Histograms snapshot;
  GetSnapshot(std::string(), &snapshot);

  std::string output = "{\"histograms\":[";
  const char* sep = "";
  for (const HistogramBase* const histogram : snapshot) {
    output += sep;
    sep = ",";
    std::string json;
    histogram->WriteJSON(&json, verbosity_level);
    output += json;
  }
  output += "]}";
  return output;
}

// static
void StatisticsRecorder::GetHistograms(Histograms* output) {
  base::AutoLock auto_lock(lock_.Get());
  if (!histograms_)
    return;

  for (const auto& entry : *histograms_) {
    output->push_back(entry.second);
  }
}

// static
void StatisticsRecorder::GetBucketRanges(
    std::vector<const BucketRanges*>* output) {
  base::AutoLock auto_lock(lock_.Get());
  if (!ranges_)
    return;

  for (const BucketRanges* const p : *ranges_) {
    output->push_back(p);
  }
}

// static
HistogramBase* StatisticsRecorder::FindHistogram(base::StringPiece name) {
  // This must be called *before* the lock is acquired below because it will
  // call back into this object to register histograms. Those called methods
  // will acquire the lock at that time.
  ImportGlobalPersistentHistograms();

  base::AutoLock auto_lock(lock_.Get());
  if (!histograms_)
    return nullptr;

  const HistogramMap::const_iterator it = histograms_->find(name);
  return it != histograms_->end() ? it->second : nullptr;
}

// static
void StatisticsRecorder::ImportProvidedHistograms() {
  if (!providers_)
    return;

  // Merge histogram data from each provider in turn.
  for (const WeakPtr<HistogramProvider>& provider : *providers_) {
    // Weak-pointer may be invalid if the provider was destructed, though they
    // generally never are.
    if (provider)
      provider->MergeHistogramDeltas();
  }
}

// static
void StatisticsRecorder::PrepareDeltas(
    bool include_persistent,
    HistogramBase::Flags flags_to_set,
    HistogramBase::Flags required_flags,
    HistogramSnapshotManager* snapshot_manager) {
  if (include_persistent)
    ImportGlobalPersistentHistograms();

  Histograms known = GetKnownHistograms(include_persistent);
  std::sort(known.begin(), known.end(), &HistogramNameLesser);
  snapshot_manager->PrepareDeltas(known, flags_to_set, required_flags);
}

// static
void StatisticsRecorder::InitLogOnShutdown() {
  if (!histograms_)
    return;

  base::AutoLock auto_lock(lock_.Get());
  g_statistics_recorder_.Get().InitLogOnShutdownWithoutLock();
}

// static
void StatisticsRecorder::GetSnapshot(const std::string& query,
                                     Histograms* snapshot) {
  // This must be called *before* the lock is acquired below because it will
  // call back into this object to register histograms. Those called methods
  // will acquire the lock at that time.
  ImportGlobalPersistentHistograms();

  base::AutoLock auto_lock(lock_.Get());
  if (!histograms_)
    return;

  // Need a c-string query for comparisons against c-string histogram name.
  const char* query_string = query.c_str();

  for (const auto& entry : *histograms_) {
    if (strstr(entry.second->histogram_name(), query_string) != nullptr)
      snapshot->push_back(entry.second);
  }
}

// static
bool StatisticsRecorder::SetCallback(
    const std::string& name,
    const StatisticsRecorder::OnSampleCallback& cb) {
  DCHECK(!cb.is_null());
  base::AutoLock auto_lock(lock_.Get());
  if (!histograms_)
    return false;

  if (!callbacks_->insert({name, cb}).second)
    return false;

  const HistogramMap::const_iterator it = histograms_->find(name);
  if (it != histograms_->end())
    it->second->SetFlags(HistogramBase::kCallbackExists);

  return true;
}

// static
void StatisticsRecorder::ClearCallback(const std::string& name) {
  base::AutoLock auto_lock(lock_.Get());
  if (!histograms_)
    return;

  callbacks_->erase(name);

  // We also clear the flag from the histogram (if it exists).
  const HistogramMap::const_iterator it = histograms_->find(name);
  if (it != histograms_->end())
    it->second->ClearFlags(HistogramBase::kCallbackExists);
}

// static
StatisticsRecorder::OnSampleCallback StatisticsRecorder::FindCallback(
    const std::string& name) {
  base::AutoLock auto_lock(lock_.Get());
  if (!histograms_)
    return OnSampleCallback();

  const auto it = callbacks_->find(name);
  return it != callbacks_->end() ? it->second : OnSampleCallback();
}

// static
size_t StatisticsRecorder::GetHistogramCount() {
  base::AutoLock auto_lock(lock_.Get());
  return histograms_ ? histograms_->size() : 0;
}

// static
void StatisticsRecorder::ForgetHistogramForTesting(base::StringPiece name) {
  if (!histograms_)
    return;

  const HistogramMap::iterator found = histograms_->find(name);
  if (found == histograms_->end())
    return;

  HistogramBase* const base = found->second;
  if (base->GetHistogramType() != SPARSE_HISTOGRAM) {
    // When forgetting a histogram, it's likely that other information is
    // also becoming invalid. Clear the persistent reference that may no
    // longer be valid. There's no danger in this as, at worst, duplicates
    // will be created in persistent memory.
    static_cast<Histogram*>(base)->bucket_ranges()->set_persistent_reference(0);
  }

  histograms_->erase(found);
}

// static
std::unique_ptr<StatisticsRecorder>
StatisticsRecorder::CreateTemporaryForTesting() {
  return WrapUnique(new StatisticsRecorder());
}

// static
void StatisticsRecorder::UninitializeForTesting() {
  // Stop now if it's never been initialized.
  if (!histograms_)
    return;

  // Get the global instance and destruct it. It's held in static memory so
  // can't "delete" it; call the destructor explicitly.
  DCHECK(g_statistics_recorder_.private_instance_);
  g_statistics_recorder_.Get().~StatisticsRecorder();

  // Now the ugly part. There's no official way to release a LazyInstance once
  // created so it's necessary to clear out an internal variable which
  // shouldn't be publicly visible but is for initialization reasons.
  g_statistics_recorder_.private_instance_ = 0;
}

// static
void StatisticsRecorder::SetRecordChecker(
    std::unique_ptr<RecordHistogramChecker> record_checker) {
  record_checker_ = record_checker.release();
}

// static
bool StatisticsRecorder::ShouldRecordHistogram(uint64_t histogram_hash) {
  return !record_checker_ || record_checker_->ShouldRecord(histogram_hash);
}

// static
StatisticsRecorder::Histograms StatisticsRecorder::GetKnownHistograms(
    bool include_persistent) {
  Histograms known;
  base::AutoLock auto_lock(lock_.Get());
  if (!histograms_ || histograms_->empty())
    return known;

  known.reserve(histograms_->size());
  for (const auto& h : *histograms_) {
    if (include_persistent ||
        (h.second->flags() & HistogramBase::kIsPersistent) == 0)
      known.push_back(h.second);
  }

  return known;
}

// static
void StatisticsRecorder::ImportGlobalPersistentHistograms() {
  if (!histograms_)
    return;

  // Import histograms from known persistent storage. Histograms could have been
  // added by other processes and they must be fetched and recognized locally.
  // If the persistent memory segment is not shared between processes, this call
  // does nothing.
  if (GlobalHistogramAllocator* allocator = GlobalHistogramAllocator::Get())
    allocator->ImportHistogramsToStatisticsRecorder();
}

// This singleton instance should be started during the single threaded portion
// of main(), and hence it is not thread safe. It initializes globals to provide
// support for all future calls.
StatisticsRecorder::StatisticsRecorder() {
  base::AutoLock auto_lock(lock_.Get());

  existing_histograms_.reset(histograms_);
  existing_callbacks_.reset(callbacks_);
  existing_ranges_.reset(ranges_);
  existing_providers_.reset(providers_);
  existing_record_checker_.reset(record_checker_);

  histograms_ = new HistogramMap;
  callbacks_ = new CallbackMap;
  ranges_ = new RangesMap;
  providers_ = new HistogramProviders;
  record_checker_ = nullptr;

  InitLogOnShutdownWithoutLock();
}

void StatisticsRecorder::InitLogOnShutdownWithoutLock() {
  if (!vlog_initialized_ && VLOG_IS_ON(1)) {
    vlog_initialized_ = true;
    AtExitManager::RegisterCallback(&DumpHistogramsToVlog, this);
  }
}

// static
void StatisticsRecorder::Reset() {
  // Declared before auto_lock to ensure correct destruction order.
  std::unique_ptr<HistogramMap> histograms_deleter;
  std::unique_ptr<CallbackMap> callbacks_deleter;
  std::unique_ptr<RangesMap> ranges_deleter;
  std::unique_ptr<HistogramProviders> providers_deleter;
  std::unique_ptr<RecordHistogramChecker> record_checker_deleter;
  base::AutoLock auto_lock(lock_.Get());
  histograms_deleter.reset(histograms_);
  callbacks_deleter.reset(callbacks_);
  ranges_deleter.reset(ranges_);
  providers_deleter.reset(providers_);
  record_checker_deleter.reset(record_checker_);
  histograms_ = nullptr;
  callbacks_ = nullptr;
  ranges_ = nullptr;
  providers_ = nullptr;
  record_checker_ = nullptr;

  // We are going to leak the histograms and the ranges.
}

// static
void StatisticsRecorder::DumpHistogramsToVlog(void* instance) {
  std::string output;
  StatisticsRecorder::WriteGraph(std::string(), &output);
  VLOG(1) << output;
}

// static
StatisticsRecorder::HistogramMap* StatisticsRecorder::histograms_ = nullptr;
// static
StatisticsRecorder::CallbackMap* StatisticsRecorder::callbacks_ = nullptr;
// static
StatisticsRecorder::RangesMap* StatisticsRecorder::ranges_ = nullptr;
// static
StatisticsRecorder::HistogramProviders* StatisticsRecorder::providers_ =
    nullptr;
// static
RecordHistogramChecker* StatisticsRecorder::record_checker_ = nullptr;
// static
base::LazyInstance<base::Lock>::Leaky StatisticsRecorder::lock_ =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace base
