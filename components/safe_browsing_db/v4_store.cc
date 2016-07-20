// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/stringprintf.h"
#include "components/safe_browsing_db/v4_store.h"
#include "components/safe_browsing_db/v4_store.pb.h"

namespace safe_browsing {

namespace {
const uint32_t kFileMagic = 0x600D71FE;

const uint32_t kFileVersion = 9;

// The minimum expected size (in bytes) of a hash-prefix.
const uint32_t kMinHashPrefixLength = 4;

// The maximum expected size (in bytes) of a hash-prefix. This represents the
// length of a SHA256 hash.
const uint32_t kMaxHashPrefixLength = 32;

void RecordStoreReadResult(StoreReadResult result) {
  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.V4StoreReadResult", result,
                            STORE_READ_RESULT_MAX);
}

void RecordStoreWriteResult(StoreWriteResult result) {
  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.V4StoreWriteResult", result,
                            STORE_WRITE_RESULT_MAX);
}

void RecordApplyUpdateResult(ApplyUpdateResult result) {
  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.V4ApplyUpdateResult", result,
                            APPLY_UPDATE_RESULT_MAX);
}

void RecordApplyUpdateResultWhenReadingFromDisk(ApplyUpdateResult result) {
  UMA_HISTOGRAM_ENUMERATION(
      "SafeBrowsing.V4ApplyUpdateResultWhenReadingFromDisk", result,
      APPLY_UPDATE_RESULT_MAX);
}

// Returns the name of the temporary file used to buffer data for
// |filename|.  Exported for unit tests.
const base::FilePath TemporaryFileForFilename(const base::FilePath& filename) {
  return base::FilePath(filename.value() + FILE_PATH_LITERAL("_new"));
}

}  // namespace

using ::google::protobuf::RepeatedField;
using ::google::protobuf::RepeatedPtrField;
using ::google::protobuf::int32;

std::ostream& operator<<(std::ostream& os, const V4Store& store) {
  os << store.DebugString();
  return os;
}

V4Store* V4StoreFactory::CreateV4Store(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const base::FilePath& store_path) {
  V4Store* new_store = new V4Store(task_runner, store_path);
  new_store->Initialize();
  return new_store;
}

void V4Store::Initialize() {
  // If a state already exists, don't re-initilize.
  DCHECK(state_.empty());

  StoreReadResult store_read_result = ReadFromDisk();
  RecordStoreReadResult(store_read_result);
}

V4Store::V4Store(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
                 const base::FilePath& store_path)
    : store_path_(store_path), task_runner_(task_runner) {}

V4Store::~V4Store() {}

std::string V4Store::DebugString() const {
  std::string state_base64;
  base::Base64Encode(state_, &state_base64);

  return base::StringPrintf("path: %" PRIsFP "; state: %s",
                            store_path_.value().c_str(), state_base64.c_str());
}

bool V4Store::Reset() {
  // TODO(vakh): Implement skeleton.
  state_ = "";
  return true;
}

void V4Store::ApplyUpdate(
    std::unique_ptr<ListUpdateResponse> response,
    const scoped_refptr<base::SingleThreadTaskRunner>& callback_task_runner,
    UpdatedStoreReadyCallback callback) {
  std::unique_ptr<V4Store> new_store(
      new V4Store(this->task_runner_, this->store_path_));
  new_store->state_ = response->new_client_state();
  // TODO(vakh):
  // 1. Done: Merge the old store and the new update in new_store.
  // 2. Create a ListUpdateResponse containing RICE encoded hash-prefixes and
  // response_type as FULL_UPDATE, and write that to disk.
  // 3. Remove this if condition after completing 1. and 2.
  HashPrefixMap hash_prefix_map;
  ApplyUpdateResult apply_update_result;
  if (response->response_type() == ListUpdateResponse::PARTIAL_UPDATE) {
    const RepeatedField<int32>* raw_removals = nullptr;
    size_t removals_size = response->removals_size();
    DCHECK_LE(removals_size, 1u);
    if (removals_size == 1) {
      const ThreatEntrySet& removal = response->removals().Get(0);
      // TODO(vakh): Allow other compression types.
      // See: https://bugs.chromium.org/p/chromium/issues/detail?id=624567
      DCHECK_EQ(RAW, removal.compression_type());
      raw_removals = &removal.raw_indices().indices();
    }

    apply_update_result = UpdateHashPrefixMapFromAdditions(
        response->additions(), &hash_prefix_map);
    if (apply_update_result == APPLY_UPDATE_SUCCESS) {
      apply_update_result = new_store->MergeUpdate(
          hash_prefix_map_, hash_prefix_map, raw_removals);
    }
    // TODO(vakh): Generate the updated ListUpdateResponse to write to disk.
  } else if (response->response_type() == ListUpdateResponse::FULL_UPDATE) {
    apply_update_result = UpdateHashPrefixMapFromAdditions(
        response->additions(), &hash_prefix_map);
    if (apply_update_result == APPLY_UPDATE_SUCCESS) {
      new_store->hash_prefix_map_ = hash_prefix_map;
      RecordStoreWriteResult(new_store->WriteToDisk(std::move(response)));
    }
  } else {
    apply_update_result = UNEXPECTED_RESPONSE_TYPE_FAILURE;
    NOTREACHED() << "Unexpected response type: " << response->response_type();
  }

  if (apply_update_result == APPLY_UPDATE_SUCCESS) {
    // new_store is done updating, pass it to the callback.
    callback_task_runner->PostTask(
        FROM_HERE, base::Bind(callback, base::Passed(&new_store)));
  } else {
    // new_store failed updating. Pass a nullptr to the callback.
    callback_task_runner->PostTask(FROM_HERE, base::Bind(callback, nullptr));
  }

  RecordApplyUpdateResult(apply_update_result);
}

// static
ApplyUpdateResult V4Store::UpdateHashPrefixMapFromAdditions(
    const RepeatedPtrField<ThreatEntrySet>& additions,
    HashPrefixMap* additions_map) {
  for (const auto& addition : additions) {
    // TODO(vakh): Allow other compression types.
    // See: https://bugs.chromium.org/p/chromium/issues/detail?id=624567
    DCHECK_EQ(RAW, addition.compression_type());

    DCHECK(addition.has_raw_hashes());
    DCHECK(addition.raw_hashes().has_raw_hashes());

    PrefixSize prefix_size = addition.raw_hashes().prefix_size();
    ApplyUpdateResult result = AddUnlumpedHashes(
        prefix_size, addition.raw_hashes().raw_hashes(), additions_map);
    if (result != APPLY_UPDATE_SUCCESS) {
      // If there was an error in updating the map, discard the update entirely.
      return result;
    }
  }
  return APPLY_UPDATE_SUCCESS;
}

// static
ApplyUpdateResult V4Store::AddUnlumpedHashes(PrefixSize prefix_size,
                                             const std::string& lumped_hashes,
                                             HashPrefixMap* additions_map) {
  if (prefix_size < kMinHashPrefixLength) {
    NOTREACHED();
    return PREFIX_SIZE_TOO_SMALL_FAILURE;
  }
  if (prefix_size > kMaxHashPrefixLength) {
    NOTREACHED();
    return PREFIX_SIZE_TOO_LARGE_FAILURE;
  }
  if (lumped_hashes.size() % prefix_size != 0) {
    return ADDITIONS_SIZE_UNEXPECTED_FAILURE;
  }
  // TODO(vakh): Figure out a way to avoid the following copy operation.
  (*additions_map)[prefix_size] = lumped_hashes;
  return APPLY_UPDATE_SUCCESS;
}

// static
bool V4Store::GetNextSmallestUnmergedPrefix(
    const HashPrefixMap& hash_prefix_map,
    const IteratorMap& iterator_map,
    HashPrefix* smallest_hash_prefix) {
  HashPrefix current_hash_prefix;
  bool has_unmerged = false;

  for (const auto& iterator_pair : iterator_map) {
    PrefixSize prefix_size = iterator_pair.first;
    HashPrefixes::const_iterator start = iterator_pair.second;
    const HashPrefixes& hash_prefixes = hash_prefix_map.at(prefix_size);
    if (prefix_size <=
        static_cast<PrefixSize>(std::distance(start, hash_prefixes.end()))) {
      current_hash_prefix = HashPrefix(start, start + prefix_size);
      if (!has_unmerged || *smallest_hash_prefix > current_hash_prefix) {
        has_unmerged = true;
        smallest_hash_prefix->swap(current_hash_prefix);
      }
    }
  }
  return has_unmerged;
}

// static
void V4Store::InitializeIteratorMap(const HashPrefixMap& hash_prefix_map,
                                    IteratorMap* iterator_map) {
  for (const auto& map_pair : hash_prefix_map) {
    (*iterator_map)[map_pair.first] = map_pair.second.begin();
  }
}

// static
void V4Store::ReserveSpaceInPrefixMap(const HashPrefixMap& other_prefixes_map,
                                      HashPrefixMap* prefix_map_to_update) {
  for (const auto& pair : other_prefixes_map) {
    PrefixSize prefix_size = pair.first;
    size_t prefix_length_to_add = pair.second.length();

    const HashPrefixes& existing_prefixes =
        (*prefix_map_to_update)[prefix_size];
    size_t existing_capacity = existing_prefixes.capacity();

    (*prefix_map_to_update)[prefix_size].reserve(existing_capacity +
                                                 prefix_length_to_add);
  }
}

ApplyUpdateResult V4Store::MergeUpdate(
    const HashPrefixMap& old_prefixes_map,
    const HashPrefixMap& additions_map,
    const RepeatedField<int32>* raw_removals) {
  DCHECK(hash_prefix_map_.empty());
  hash_prefix_map_.clear();
  ReserveSpaceInPrefixMap(old_prefixes_map, &hash_prefix_map_);
  ReserveSpaceInPrefixMap(additions_map, &hash_prefix_map_);

  IteratorMap old_iterator_map;
  HashPrefix next_smallest_prefix_old;
  InitializeIteratorMap(old_prefixes_map, &old_iterator_map);
  bool old_has_unmerged = GetNextSmallestUnmergedPrefix(
      old_prefixes_map, old_iterator_map, &next_smallest_prefix_old);

  IteratorMap additions_iterator_map;
  HashPrefix next_smallest_prefix_additions;
  InitializeIteratorMap(additions_map, &additions_iterator_map);
  bool additions_has_unmerged = GetNextSmallestUnmergedPrefix(
      additions_map, additions_iterator_map, &next_smallest_prefix_additions);

  // Classical merge sort.
  // The two constructs to merge are maps: old_prefixes_map, additions_map.
  // At least one of the maps still has elements that need to be merged into the
  // new store.

  // Keep track of the number of elements picked from the old map. This is used
  // to determine which elements to drop based on the raw_removals. Note that
  // picked is not the same as merged. A picked element isn't merged if its
  // index is on the raw_removals list.
  int total_picked_from_old = 0;
  const int* removals_iter = raw_removals ? raw_removals->begin() : nullptr;
  while (old_has_unmerged || additions_has_unmerged) {
    // If the same hash prefix appears in the existing store and the additions
    // list, something is clearly wrong. Discard the update.
    if (old_has_unmerged && additions_has_unmerged &&
        next_smallest_prefix_old == next_smallest_prefix_additions) {
      return ADDITIONS_HAS_EXISTING_PREFIX_FAILURE;
    }

    // Select which map to pick the next hash prefix from to keep the result in
    // lexographically sorted order.
    bool pick_from_old =
        old_has_unmerged &&
        (!additions_has_unmerged ||
         (next_smallest_prefix_old < next_smallest_prefix_additions));

    PrefixSize next_smallest_prefix_size;
    if (pick_from_old) {
      next_smallest_prefix_size = next_smallest_prefix_old.size();

      // Update the iterator map, which means that we have merged one hash
      // prefix of size |next_size_for_old| from the old store.
      old_iterator_map[next_smallest_prefix_size] += next_smallest_prefix_size;

      if (!raw_removals || removals_iter == raw_removals->end() ||
          *removals_iter != total_picked_from_old) {
        // Append the smallest hash to the appropriate list.
        hash_prefix_map_[next_smallest_prefix_size] += next_smallest_prefix_old;
      } else {
        // Element not added to new map. Move the removals iterator forward.
        removals_iter++;
      }

      total_picked_from_old++;

      // Find the next smallest unmerged element in the old store's map.
      old_has_unmerged = GetNextSmallestUnmergedPrefix(
          old_prefixes_map, old_iterator_map, &next_smallest_prefix_old);
    } else {
      next_smallest_prefix_size = next_smallest_prefix_additions.size();

      // Append the smallest hash to the appropriate list.
      hash_prefix_map_[next_smallest_prefix_size] +=
          next_smallest_prefix_additions;

      // Update the iterator map, which means that we have merged one hash
      // prefix of size |next_smallest_prefix_size| from the update.
      additions_iterator_map[next_smallest_prefix_size] +=
          next_smallest_prefix_size;

      // Find the next smallest unmerged element in the additions map.
      additions_has_unmerged =
          GetNextSmallestUnmergedPrefix(additions_map, additions_iterator_map,
                                        &next_smallest_prefix_additions);
    }
  }

  return (!raw_removals || removals_iter == raw_removals->end())
             ? APPLY_UPDATE_SUCCESS
             : REMOVALS_INDEX_TOO_LARGE;
}

StoreReadResult V4Store::ReadFromDisk() {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  std::string contents;
  bool read_success = base::ReadFileToString(store_path_, &contents);
  if (!read_success) {
    return FILE_UNREADABLE_FAILURE;
  }

  if (contents.empty()) {
    return FILE_EMPTY_FAILURE;
  }

  V4StoreFileFormat file_format;
  if (!file_format.ParseFromString(contents)) {
    return PROTO_PARSING_FAILURE;
  }

  if (file_format.magic_number() != kFileMagic) {
    DVLOG(1) << "Unexpected magic number found in file: "
             << file_format.magic_number();
    return UNEXPECTED_MAGIC_NUMBER_FAILURE;
  }

  UMA_HISTOGRAM_SPARSE_SLOWLY("SafeBrowsing.V4StoreVersionRead",
                              file_format.version_number());
  if (file_format.version_number() != kFileVersion) {
    DVLOG(1) << "File version incompatible: " << file_format.version_number()
             << "; expected: " << kFileVersion;
    return FILE_VERSION_INCOMPATIBLE_FAILURE;
  }

  if (!file_format.has_list_update_response()) {
    return HASH_PREFIX_INFO_MISSING_FAILURE;
  }

  const ListUpdateResponse& response = file_format.list_update_response();
  ApplyUpdateResult apply_update_result = UpdateHashPrefixMapFromAdditions(
      response.additions(), &hash_prefix_map_);
  RecordApplyUpdateResultWhenReadingFromDisk(apply_update_result);
  if (apply_update_result != APPLY_UPDATE_SUCCESS) {
    hash_prefix_map_.clear();
    return HASH_PREFIX_MAP_GENERATION_FAILURE;
  }

  state_ = response.new_client_state();
  return READ_SUCCESS;
}

StoreWriteResult V4Store::WriteToDisk(
    std::unique_ptr<ListUpdateResponse> response) const {
  // Do not write partial updates to the disk.
  // After merging the updates, the ListUpdateResponse passed to this method
  // should be a FULL_UPDATE.
  if (!response->has_response_type() ||
      response->response_type() != ListUpdateResponse::FULL_UPDATE) {
    DVLOG(1) << "response->has_response_type(): "
             << response->has_response_type();
    DVLOG(1) << "response->response_type(): " << response->response_type();
    return INVALID_RESPONSE_TYPE_FAILURE;
  }

  // Attempt writing to a temporary file first and at the end, swap the files.
  const base::FilePath new_filename = TemporaryFileForFilename(store_path_);

  V4StoreFileFormat file_format;
  file_format.set_magic_number(kFileMagic);
  file_format.set_version_number(kFileVersion);
  ListUpdateResponse* response_to_write =
      file_format.mutable_list_update_response();
  response_to_write->Swap(response.get());
  std::string file_format_string;
  file_format.SerializeToString(&file_format_string);
  size_t written = base::WriteFile(new_filename, file_format_string.data(),
                                   file_format_string.size());
  DCHECK_EQ(file_format_string.size(), written);

  if (!base::Move(new_filename, store_path_)) {
    DVLOG(1) << "store_path_: " << store_path_.value();
    return UNABLE_TO_RENAME_FAILURE;
  }

  return WRITE_SUCCESS;
}

HashPrefix V4Store::GetMatchingHashPrefix(const FullHash& full_hash) {
  // It should never be the case that more than one hash prefixes match a given
  // full hash. However, if that happens, this method returns any one of them.
  // It does not guarantee which one of those will be returned.
  DCHECK_EQ(32u, full_hash.size());
  for (const auto& pair : hash_prefix_map_) {
    const PrefixSize& prefix_size = pair.first;
    const HashPrefixes& hash_prefixes = pair.second;
    HashPrefix hash_prefix = full_hash.substr(0, prefix_size);
    if (HashPrefixMatches(hash_prefix, hash_prefixes.begin(),
                          hash_prefixes.end())) {
      return hash_prefix;
    }
  }
  return HashPrefix();
}

// static
bool V4Store::HashPrefixMatches(const HashPrefix& hash_prefix,
                                const HashPrefixes::const_iterator& begin,
                                const HashPrefixes::const_iterator& end) {
  if (begin == end) {
    return false;
  }
  size_t distance = std::distance(begin, end);
  const PrefixSize prefix_size = hash_prefix.length();
  DCHECK_EQ(0u, distance % prefix_size);
  size_t mid_prefix_index = ((distance / prefix_size) / 2) * prefix_size;
  HashPrefixes::const_iterator mid = begin + mid_prefix_index;
  HashPrefix mid_prefix = HashPrefix(mid, mid + prefix_size);
  int result = hash_prefix.compare(mid_prefix);
  if (result == 0) {
    return true;
  } else if (result < 0) {
    return HashPrefixMatches(hash_prefix, begin, mid);
  } else {
    return HashPrefixMatches(hash_prefix, mid + prefix_size, end);
  }
}

}  // namespace safe_browsing
