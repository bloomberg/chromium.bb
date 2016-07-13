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

// Returns the name of the temporary file used to buffer data for
// |filename|.  Exported for unit tests.
const base::FilePath TemporaryFileForFilename(const base::FilePath& filename) {
  return base::FilePath(filename.value() + FILE_PATH_LITERAL("_new"));
}

}  // namespace

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
  // 1. Merge the old store and the new update in new_store.
  // 2. Create a ListUpdateResponse containing RICE encoded hash-prefixes and
  // response_type as FULL_UPDATE, and write that to disk.
  // 3. Remove this if condition after completing 1. and 2.
  HashPrefixMap hash_prefix_map;
  ApplyUpdateResult apply_update_result;
  if (response->response_type() == ListUpdateResponse::PARTIAL_UPDATE) {
    for (const auto& removal : response->removals()) {
      // TODO(vakh): Allow other compression types.
      // See: https://bugs.chromium.org/p/chromium/issues/detail?id=624567
      DCHECK_EQ(RAW, removal.compression_type());
    }

    apply_update_result = UpdateHashPrefixMapFromAdditions(
        response->additions(), &hash_prefix_map);
    if (apply_update_result == APPLY_UPDATE_SUCCESS) {
      apply_update_result =
          new_store->MergeUpdate(hash_prefix_map_, hash_prefix_map);
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
    const ::google::protobuf::RepeatedPtrField<ThreatEntrySet>& additions,
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
bool V4Store::GetNextSmallestPrefixSize(const HashPrefixMap& hash_prefix_map,
                                        const CounterMap& counter_map,
                                        PrefixSize* smallest_prefix_size) {
  HashPrefix smallest_prefix, current_prefix;
  for (const auto& counter_pair : counter_map) {
    PrefixSize prefix_size = counter_pair.first;
    size_t index = counter_pair.second;
    size_t sized_index = prefix_size * index;

    const HashPrefixes& hash_prefixes = hash_prefix_map.at(prefix_size);
    if (sized_index < hash_prefixes.size()) {
      current_prefix = hash_prefixes.substr(sized_index, prefix_size);
      if (smallest_prefix.empty() || smallest_prefix > current_prefix) {
        smallest_prefix = current_prefix;
        *smallest_prefix_size = prefix_size;
      }
    }
  }
  return !smallest_prefix.empty();
}

// static
void V4Store::InitializeCounterMap(const HashPrefixMap& hash_prefix_map,
                                   CounterMap* counter_map) {
  for (const auto& map_pair : hash_prefix_map) {
    (*counter_map)[map_pair.first] = 0;
  }
}

// static
HashPrefix V4Store::GetNextUnmergedPrefixForSize(
    PrefixSize prefix_size,
    const HashPrefixMap& hash_prefix_map,
    const CounterMap& counter_map) {
  const HashPrefixes& hash_prefixes = hash_prefix_map.at(prefix_size);
  size_t index_within_list = counter_map.at(prefix_size);
  size_t sized_index = prefix_size * index_within_list;
  return hash_prefixes.substr(sized_index, prefix_size);
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

ApplyUpdateResult V4Store::MergeUpdate(const HashPrefixMap& old_prefixes_map,
                                       const HashPrefixMap& additions_map) {
  DCHECK(hash_prefix_map_.empty());
  hash_prefix_map_.clear();
  ReserveSpaceInPrefixMap(old_prefixes_map, &hash_prefix_map_);
  ReserveSpaceInPrefixMap(additions_map, &hash_prefix_map_);

  PrefixSize next_size_for_old;
  CounterMap old_counter_map;
  InitializeCounterMap(old_prefixes_map, &old_counter_map);
  bool old_has_unmerged = GetNextSmallestPrefixSize(
      old_prefixes_map, old_counter_map, &next_size_for_old);

  PrefixSize next_size_for_additions;
  CounterMap additions_counter_map;
  InitializeCounterMap(additions_map, &additions_counter_map);
  bool additions_has_unmerged = GetNextSmallestPrefixSize(
      additions_map, additions_counter_map, &next_size_for_additions);

  // Classical merge sort.
  // The two constructs to merge are maps: old_prefixes_map, additions_map.

  HashPrefix next_smallest_prefix_old, next_smallest_prefix_additions;
  // At least one of the maps still has elements that need to be merged into the
  // new store.
  while (old_has_unmerged || additions_has_unmerged) {
    // Old map still has elements that need to be merged.
    if (old_has_unmerged) {
      // Get the lexographically smallest hash prefix from the old store.
      next_smallest_prefix_old = GetNextUnmergedPrefixForSize(
          next_size_for_old, old_prefixes_map, old_counter_map);
    }

    // Additions map still has elements that need to be merged.
    if (additions_has_unmerged) {
      // Get the lexographically smallest hash prefix from the additions in the
      // latest update from the server.
      next_smallest_prefix_additions = GetNextUnmergedPrefixForSize(
          next_size_for_additions, additions_map, additions_counter_map);
    }

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

    if (pick_from_old) {
      hash_prefix_map_[next_size_for_old] += next_smallest_prefix_old;

      // Update the counter map, which means that we have merged one hash
      // prefix of size |next_size_for_old| from the old store.
      old_counter_map[next_size_for_old]++;

      // Find the next smallest unmerged element in the old store's map.
      old_has_unmerged = GetNextSmallestPrefixSize(
          old_prefixes_map, old_counter_map, &next_size_for_old);
    } else {
      hash_prefix_map_[next_size_for_additions] +=
          next_smallest_prefix_additions;

      // Update the counter map, which means that we have merged one hash
      // prefix of size |next_size_for_additions| from the update.
      additions_counter_map[next_size_for_additions]++;

      // Find the next smallest unmerged element in the additions map.
      additions_has_unmerged = GetNextSmallestPrefixSize(
          additions_map, additions_counter_map, &next_size_for_additions);
    }
  }

  return APPLY_UPDATE_SUCCESS;
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

  ListUpdateResponse list_update_response = file_format.list_update_response();
  state_ = list_update_response.new_client_state();
  // TODO(vakh): Do more with what's read from the disk.

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

}  // namespace safe_browsing
