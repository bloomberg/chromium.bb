// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/call_stack_profile_builder.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <string>
#include <tuple>
#include <utility>

#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/metrics_hashes.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "components/metrics/call_stack_profile_encoding.h"

namespace metrics {

namespace {

// Only used by child processes.
base::LazyInstance<ChildCallStackProfileCollector>::Leaky
    g_child_call_stack_profile_collector = LAZY_INSTANCE_INITIALIZER;

base::RepeatingCallback<void(base::TimeTicks, SampledProfile)>&
GetBrowserProcessReceiverCallbackInstance() {
  static base::NoDestructor<
      base::RepeatingCallback<void(base::TimeTicks, SampledProfile)>>
      instance;
  return *instance;
}

// Convert |filename| to its MD5 hash.
uint64_t HashModuleFilename(const base::FilePath& filename) {
  const base::FilePath::StringType basename = filename.BaseName().value();
  // Copy the bytes in basename into a string buffer.
  size_t basename_length_in_bytes =
      basename.size() * sizeof(base::FilePath::CharType);
  std::string name_bytes(basename_length_in_bytes, '\0');
  memcpy(&name_bytes[0], &basename[0], basename_length_in_bytes);
  return base::HashMetricName(name_bytes);
}

std::map<uint64_t, int64_t> CreateMetadataMap(
    base::ProfileBuilder::MetadataItemArray items,
    size_t item_count) {
  std::map<uint64_t, int64_t> item_map;
  for (size_t i = 0; i < item_count; ++i) {
    item_map[items[i].name_hash] = items[i].value;
  }
  return item_map;
}

// Returns all metadata items with new values in the current sample.
std::map<uint64_t, int64_t> GetNewOrModifiedMetadataItems(
    const std::map<uint64_t, int64_t>& current_items,
    const std::map<uint64_t, int64_t>& previous_items) {
  std::map<uint64_t, int64_t> new_or_modified_items;
  // By default, std::pairs are sorted by the first then second pair elements
  // and therefore pairs with either element differing are treated as different.
  std::set_difference(
      current_items.begin(), current_items.end(), previous_items.begin(),
      previous_items.end(),
      std::inserter(new_or_modified_items, new_or_modified_items.begin()));
  return new_or_modified_items;
}

// Returns all metadata items deleted since the previous sample.
std::map<uint64_t, int64_t> GetDeletedMetadataItems(
    const std::map<uint64_t, int64_t>& current_items,
    const std::map<uint64_t, int64_t>& previous_items) {
  std::map<uint64_t, int64_t> deleted_items;
  // By default, std::pairs are sorted by the first then second pair elements
  // and therefore pairs with either element differing are treated as different.
  //
  // To find removed items, we need to override this comparator to do a set
  // subtraction based only on the item name hashes, ignoring the item values.
  //
  // The set_difference algorithm requires that the items in the set already be
  // sorted according to whatever comparator is passed to set_difference.
  // Because our new sort order is just a looser version of the existing set
  // sort order, we can find the set_difference here without creating a new set.
  auto name_hash_comparator = [](const std::pair<uint64_t, int64_t>& lhs,
                                 const std::pair<uint64_t, int64_t>& rhs) {
    return lhs.first < rhs.first;
  };
  std::set_difference(previous_items.begin(), previous_items.end(),
                      current_items.begin(), current_items.end(),
                      std::inserter(deleted_items, deleted_items.begin()),
                      name_hash_comparator);
  return deleted_items;
}

}  // namespace

CallStackProfileBuilder::CallStackProfileBuilder(
    const CallStackProfileParams& profile_params,
    const WorkIdRecorder* work_id_recorder,
    base::OnceClosure completed_callback)
    : work_id_recorder_(work_id_recorder),
      profile_start_time_(base::TimeTicks::Now()) {
  completed_callback_ = std::move(completed_callback);
  sampled_profile_.set_process(
      ToExecutionContextProcess(profile_params.process));
  sampled_profile_.set_thread(ToExecutionContextThread(profile_params.thread));
  sampled_profile_.set_trigger_event(
      ToSampledProfileTriggerEvent(profile_params.trigger));
}

CallStackProfileBuilder::~CallStackProfileBuilder() = default;

base::ModuleCache* CallStackProfileBuilder::GetModuleCache() {
  return &module_cache_;
}

// This function is invoked on the profiler thread while the target thread is
// suspended so must not take any locks, including indirectly through use of
// heap allocation, LOG, CHECK, or DCHECK.
void CallStackProfileBuilder::RecordMetadata(
    base::ProfileBuilder::MetadataProvider* metadata_provider) {
  if (work_id_recorder_) {
    unsigned int work_id = work_id_recorder_->RecordWorkId();
    // A work id of 0 indicates that the message loop has not yet started.
    if (work_id != 0) {
      is_continued_work_ = (last_work_id_ == work_id);
      last_work_id_ = work_id;
    }
  }

  if (metadata_provider)
    metadata_item_count_ = metadata_provider->GetItems(&metadata_items_);
}

void CallStackProfileBuilder::OnSampleCompleted(
    std::vector<base::Frame> frames) {
  OnSampleCompleted(std::move(frames), 1, 1);
}

void CallStackProfileBuilder::OnSampleCompleted(std::vector<base::Frame> frames,
                                                size_t weight,
                                                size_t count) {
  // Write CallStackProfile::Stack protobuf message.
  CallStackProfile::Stack stack;

  for (const auto& frame : frames) {
    // keep the frame information even if its module is invalid so we have
    // visibility into how often this issue is happening on the server.
    CallStackProfile::Location* location = stack.add_frame();
    if (!frame.module)
      continue;

    // Dedup modules.
    auto module_loc = module_index_.find(frame.module);
    if (module_loc == module_index_.end()) {
      modules_.push_back(frame.module);
      size_t index = modules_.size() - 1;
      module_loc = module_index_.emplace(frame.module, index).first;
    }

    // Write CallStackProfile::Location protobuf message.
    ptrdiff_t module_offset =
        reinterpret_cast<const char*>(frame.instruction_pointer) -
        reinterpret_cast<const char*>(frame.module->GetBaseAddress());
    DCHECK_GE(module_offset, 0);
    location->set_address(static_cast<uint64_t>(module_offset));
    location->set_module_id_index(module_loc->second);
  }

  CallStackProfile* call_stack_profile =
      sampled_profile_.mutable_call_stack_profile();

  // Dedup Stacks.
  auto stack_loc = stack_index_.find(&stack);
  if (stack_loc == stack_index_.end()) {
    *call_stack_profile->add_stack() = std::move(stack);
    int stack_index = call_stack_profile->stack_size() - 1;
    // It is safe to store the Stack pointer because the repeated message
    // representation ensures pointer stability.
    stack_loc = stack_index_
                    .emplace(call_stack_profile->mutable_stack(stack_index),
                             stack_index)
                    .first;
  }

  // Write CallStackProfile::StackSample protobuf message.
  CallStackProfile::StackSample* stack_sample_proto =
      call_stack_profile->add_stack_sample();
  stack_sample_proto->set_stack_index(stack_loc->second);
  if (weight != 1)
    stack_sample_proto->set_weight(weight);
  if (count != 1)
    stack_sample_proto->set_count(count);
  if (is_continued_work_)
    stack_sample_proto->set_continued_work(is_continued_work_);

  AddSampleMetadata(call_stack_profile, stack_sample_proto);
}

void CallStackProfileBuilder::OnProfileCompleted(
    base::TimeDelta profile_duration,
    base::TimeDelta sampling_period) {
  // Build the SampledProfile protobuf message.
  CallStackProfile* call_stack_profile =
      sampled_profile_.mutable_call_stack_profile();
  call_stack_profile->set_profile_duration_ms(
      profile_duration.InMilliseconds());
  call_stack_profile->set_sampling_period_ms(sampling_period.InMilliseconds());

  // Write CallStackProfile::ModuleIdentifier protobuf message.
  for (const auto* module : modules_) {
    CallStackProfile::ModuleIdentifier* module_id =
        call_stack_profile->add_module_id();
    module_id->set_build_id(module->GetId());
    module_id->set_name_md5_prefix(
        HashModuleFilename(module->GetDebugBasename()));
  }

  PassProfilesToMetricsProvider(std::move(sampled_profile_));

  // Run the completed callback if there is one.
  if (!completed_callback_.is_null())
    std::move(completed_callback_).Run();

  // Clear the caches.
  stack_index_.clear();
  module_index_.clear();
  modules_.clear();
}

// static
void CallStackProfileBuilder::SetBrowserProcessReceiverCallback(
    const base::RepeatingCallback<void(base::TimeTicks, SampledProfile)>&
        callback) {
  GetBrowserProcessReceiverCallbackInstance() = callback;
}

// static
void CallStackProfileBuilder::SetParentProfileCollectorForChildProcess(
    mojo::PendingRemote<metrics::mojom::CallStackProfileCollector>
        browser_interface) {
  g_child_call_stack_profile_collector.Get().SetParentProfileCollector(
      std::move(browser_interface));
}

void CallStackProfileBuilder::PassProfilesToMetricsProvider(
    SampledProfile sampled_profile) {
  if (sampled_profile.process() == BROWSER_PROCESS) {
    GetBrowserProcessReceiverCallbackInstance().Run(profile_start_time_,
                                                    std::move(sampled_profile));
  } else {
    g_child_call_stack_profile_collector.Get()
        .ChildCallStackProfileCollector::Collect(profile_start_time_,
                                                 std::move(sampled_profile));
  }
}

bool CallStackProfileBuilder::StackComparer::operator()(
    const CallStackProfile::Stack* stack1,
    const CallStackProfile::Stack* stack2) const {
  return std::lexicographical_compare(
      stack1->frame().begin(), stack1->frame().end(), stack2->frame().begin(),
      stack2->frame().end(),
      [](const CallStackProfile::Location& loc1,
         const CallStackProfile::Location& loc2) {
        return std::make_pair(loc1.address(), loc1.module_id_index()) <
               std::make_pair(loc2.address(), loc2.module_id_index());
      });
}

void CallStackProfileBuilder::AddSampleMetadata(
    CallStackProfile* profile,
    CallStackProfile::StackSample* sample) {
  std::map<uint64_t, int64_t> current_items =
      CreateMetadataMap(metadata_items_, metadata_item_count_);

  for (auto item :
       GetNewOrModifiedMetadataItems(current_items, previous_items_)) {
    size_t name_hash_index = MaybeAddNameHashToProfile(profile, item.first);

    CallStackProfile::MetadataItem* profile_item = sample->add_metadata();
    profile_item->set_name_hash_index(name_hash_index);
    profile_item->set_value(item.second);
  }

  for (auto item : GetDeletedMetadataItems(current_items, previous_items_)) {
    size_t name_hash_index = MaybeAddNameHashToProfile(profile, item.first);

    CallStackProfile::MetadataItem* profile_item = sample->add_metadata();
    profile_item->set_name_hash_index(name_hash_index);
    // Leave the value empty to indicate that the item was deleted.
  }

  previous_items_ = std::move(current_items);
  metadata_item_count_ = 0;
}

size_t CallStackProfileBuilder::MaybeAddNameHashToProfile(
    CallStackProfile* profile,
    uint64_t name_hash) {
  std::unordered_map<uint64_t, int>::iterator it;
  bool inserted;
  int next_item_index = profile->metadata_name_hash_size();

  std::tie(it, inserted) =
      metadata_hashes_cache_.emplace(name_hash, next_item_index);
  if (inserted)
    profile->add_metadata_name_hash(name_hash);

  return it->second;
}

}  // namespace metrics
