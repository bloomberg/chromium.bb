// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "components/safe_browsing_db/v4_rice.h"
#include "components/safe_browsing_db/v4_store.h"
#include "components/safe_browsing_db/v4_store.pb.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"

using base::TimeTicks;

namespace safe_browsing {

namespace {

const uint32_t kFileMagic = 0x600D71FE;

const uint32_t kFileVersion = 9;

std::string GetUmaSuffixForStore(const base::FilePath& file_path) {
  return base::StringPrintf(
      ".%" PRIsFP, file_path.BaseName().RemoveExtension().value().c_str());
}

void RecordTimeWithAndWithoutStore(const std::string& metric,
                                   base::TimeDelta time,
                                   const base::FilePath& file_path) {
  std::string suffix = GetUmaSuffixForStore(file_path);

  // The histograms below are a modified expansion of the
  // UMA_HISTOGRAM_LONG_TIMES macro adapted to allow for a dynamically suffixed
  // histogram name.
  // Note: The factory creates and owns the histogram.
  const int kBucketCount = 100;
  base::HistogramBase* histogram = base::Histogram::FactoryTimeGet(
      metric, base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromMinutes(1), kBucketCount,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  if (histogram) {
    histogram->AddTime(time);
  }

  base::HistogramBase* histogram_suffix = base::Histogram::FactoryTimeGet(
      metric + suffix, base::TimeDelta::FromMilliseconds(1),
      base::TimeDelta::FromMinutes(1), kBucketCount,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  if (histogram_suffix) {
    histogram_suffix->AddTime(time);
  }
}

void RecordAddUnlumpedHashesTime(base::TimeDelta time) {
  UMA_HISTOGRAM_LONG_TIMES("SafeBrowsing.V4AddUnlumpedHashesTime", time);
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

void RecordDecodeAdditionsResult(V4DecodeResult result) {
  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.V4DecodeAdditionsResult", result,
                            DECODE_RESULT_MAX);
}

void RecordDecodeAdditionsTime(base::TimeDelta time,
                               const base::FilePath& file_path) {
  RecordTimeWithAndWithoutStore("SafeBrowsing.V4DecodeAdditionsTime", time,
                                file_path);
}

void RecordDecodeRemovalsResult(V4DecodeResult result) {
  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.V4DecodeRemovalsResult", result,
                            DECODE_RESULT_MAX);
}

void RecordDecodeRemovalsTime(base::TimeDelta time,
                              const base::FilePath& file_path) {
  RecordTimeWithAndWithoutStore("SafeBrowsing.V4DecodeRemovalsTime", time,
                                file_path);
}

void RecordMergeUpdateTime(base::TimeDelta time,
                           const base::FilePath& file_path) {
  RecordTimeWithAndWithoutStore("SafeBrowsing.V4MergeUpdateTime", time,
                                file_path);
}

void RecordProcessFullUpdateTime(base::TimeDelta time,
                                 const base::FilePath& file_path) {
  RecordTimeWithAndWithoutStore("SafeBrowsing.V4ProcessFullUpdateTime", time,
                                file_path);
}

void RecordProcessPartialUpdateTime(base::TimeDelta time,
                                    const base::FilePath& file_path) {
  RecordTimeWithAndWithoutStore("SafeBrowsing.V4ProcessPartialUpdateTime", time,
                                file_path);
}

void RecordReadFromDiskTime(base::TimeDelta time,
                            const base::FilePath& file_path) {
  RecordTimeWithAndWithoutStore("SafeBrowsing.V4ReadFromDiskTime", time,
                                file_path);
}

void RecordStoreReadResult(StoreReadResult result) {
  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.V4StoreReadResult", result,
                            STORE_READ_RESULT_MAX);
}

void RecordStoreWriteResult(StoreWriteResult result) {
  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.V4StoreWriteResult", result,
                            STORE_WRITE_RESULT_MAX);
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

ApplyUpdateResult V4Store::ProcessPartialUpdateAndWriteToDisk(
    const HashPrefixMap& hash_prefix_map_old,
    std::unique_ptr<ListUpdateResponse> response) {
  DCHECK(response->has_response_type());
  DCHECK_EQ(ListUpdateResponse::PARTIAL_UPDATE, response->response_type());

  TimeTicks before = TimeTicks::Now();
  ApplyUpdateResult result = ProcessUpdate(hash_prefix_map_old, response);
  if (result == APPLY_UPDATE_SUCCESS) {
    RecordProcessPartialUpdateTime(TimeTicks::Now() - before, store_path_);
    // TODO(vakh): Create a ListUpdateResponse containing RICE encoded
    // hash prefixes and response_type as FULL_UPDATE, and write that to disk.
  }
  return result;
}

ApplyUpdateResult V4Store::ProcessFullUpdateAndWriteToDisk(
    std::unique_ptr<ListUpdateResponse> response) {
  TimeTicks before = TimeTicks::Now();
  ApplyUpdateResult result = ProcessFullUpdate(response);
  if (result == APPLY_UPDATE_SUCCESS) {
    RecordStoreWriteResult(WriteToDisk(std::move(response)));
    RecordProcessFullUpdateTime(TimeTicks::Now() - before, store_path_);
  }
  return result;
}

ApplyUpdateResult V4Store::ProcessFullUpdate(
    const std::unique_ptr<ListUpdateResponse>& response) {
  DCHECK(response->has_response_type());
  DCHECK_EQ(ListUpdateResponse::FULL_UPDATE, response->response_type());
  // TODO(vakh): For a full update, we don't need to process the update in
  // lexographical order to store it, but we do need to do that for calculating
  // checksum. It might save some CPU cycles to store the full update as-is and
  // walk the list of hash prefixes in lexographical order only for checksum
  // calculation.
  return ProcessUpdate(HashPrefixMap(), response);
}

ApplyUpdateResult V4Store::ProcessUpdate(
    const HashPrefixMap& hash_prefix_map_old,
    const std::unique_ptr<ListUpdateResponse>& response) {
  const RepeatedField<int32>* raw_removals = nullptr;
  RepeatedField<int32> rice_removals;
  size_t removals_size = response->removals_size();
  DCHECK_LE(removals_size, 1u);
  if (removals_size == 1) {
    const ThreatEntrySet& removal = response->removals().Get(0);
    const CompressionType compression_type = removal.compression_type();
    if (compression_type == RAW) {
      raw_removals = &removal.raw_indices().indices();
    } else if (compression_type == RICE) {
      DCHECK(removal.has_rice_indices());

      const RiceDeltaEncoding& rice_indices = removal.rice_indices();
      TimeTicks before = TimeTicks::Now();
      V4DecodeResult decode_result = V4RiceDecoder::DecodeIntegers(
          rice_indices.first_value(), rice_indices.rice_parameter(),
          rice_indices.num_entries(), rice_indices.encoded_data(),
          &rice_removals);

      RecordDecodeRemovalsResult(decode_result);
      if (decode_result != DECODE_SUCCESS) {
        return RICE_DECODING_FAILURE;
      }
      RecordDecodeRemovalsTime(TimeTicks::Now() - before, store_path_);
      raw_removals = &rice_removals;
    } else {
      NOTREACHED() << "Unexpected compression_type type: " << compression_type;
      return UNEXPECTED_COMPRESSION_TYPE_REMOVALS_FAILURE;
    }
  }

  HashPrefixMap hash_prefix_map;
  ApplyUpdateResult apply_update_result =
      UpdateHashPrefixMapFromAdditions(response->additions(), &hash_prefix_map);
  if (apply_update_result != APPLY_UPDATE_SUCCESS) {
    return apply_update_result;
  }

  std::string expected_checksum;
  if (response->has_checksum() && response->checksum().has_sha256()) {
    expected_checksum = response->checksum().sha256();
  }

  TimeTicks before = TimeTicks::Now();
  apply_update_result = MergeUpdate(hash_prefix_map_old, hash_prefix_map,
                                    raw_removals, expected_checksum);
  if (apply_update_result != APPLY_UPDATE_SUCCESS) {
    return apply_update_result;
  }
  RecordMergeUpdateTime(TimeTicks::Now() - before, store_path_);

  state_ = response->new_client_state();
  return APPLY_UPDATE_SUCCESS;
}

void V4Store::ApplyUpdate(
    std::unique_ptr<ListUpdateResponse> response,
    const scoped_refptr<base::SingleThreadTaskRunner>& callback_task_runner,
    UpdatedStoreReadyCallback callback) {
  std::unique_ptr<V4Store> new_store(
      new V4Store(this->task_runner_, this->store_path_));
  ApplyUpdateResult apply_update_result;
  if (response->response_type() == ListUpdateResponse::PARTIAL_UPDATE) {
    apply_update_result = new_store->ProcessPartialUpdateAndWriteToDisk(
        hash_prefix_map_, std::move(response));
  } else if (response->response_type() == ListUpdateResponse::FULL_UPDATE) {
    apply_update_result =
        new_store->ProcessFullUpdateAndWriteToDisk(std::move(response));
  } else {
    apply_update_result = UNEXPECTED_RESPONSE_TYPE_FAILURE;
    NOTREACHED() << "Failure: Unexpected response type: "
                 << response->response_type();
  }

  if (apply_update_result == APPLY_UPDATE_SUCCESS) {
    // new_store is done updating, pass it to the callback.
    callback_task_runner->PostTask(
        FROM_HERE, base::Bind(callback, base::Passed(&new_store)));
  } else {
    DVLOG(1) << "Failure: ApplyUpdate: reason: " << apply_update_result
             << "; store: " << *this;
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
    ApplyUpdateResult apply_update_result = APPLY_UPDATE_SUCCESS;
    const CompressionType compression_type = addition.compression_type();
    if (compression_type == RAW) {
      DCHECK(addition.has_raw_hashes());
      DCHECK(addition.raw_hashes().has_raw_hashes());

      apply_update_result =
          AddUnlumpedHashes(addition.raw_hashes().prefix_size(),
                            addition.raw_hashes().raw_hashes(), additions_map);
    } else if (compression_type == RICE) {
      DCHECK(addition.has_rice_hashes());

      const RiceDeltaEncoding& rice_hashes = addition.rice_hashes();
      std::vector<uint32_t> raw_hashes;
      TimeTicks before = TimeTicks::Now();
      V4DecodeResult decode_result = V4RiceDecoder::DecodePrefixes(
          rice_hashes.first_value(), rice_hashes.rice_parameter(),
          rice_hashes.num_entries(), rice_hashes.encoded_data(), &raw_hashes);
      RecordDecodeAdditionsResult(decode_result);
      if (decode_result != DECODE_SUCCESS) {
        return RICE_DECODING_FAILURE;
      } else {
        RecordDecodeAdditionsTime(TimeTicks::Now() - before, store_path_);
        char* raw_hashes_start = reinterpret_cast<char*>(raw_hashes.data());
        size_t raw_hashes_size = sizeof(uint32_t) * raw_hashes.size();

        // Rice-Golomb encoding is used to send compressed compressed 4-byte
        // hash prefixes. Hash prefixes longer than 4 bytes will not be
        // compressed, and will be served in raw format instead.
        // Source: https://developers.google.com/safe-browsing/v4/compression
        const PrefixSize kPrefixSize = 4;
        apply_update_result = AddUnlumpedHashes(kPrefixSize, raw_hashes_start,
                                                raw_hashes_size, additions_map);
      }
    } else {
      NOTREACHED() << "Unexpected compression_type type: " << compression_type;
      return UNEXPECTED_COMPRESSION_TYPE_ADDITIONS_FAILURE;
    }

    if (apply_update_result != APPLY_UPDATE_SUCCESS) {
      // If there was an error in updating the map, discard the update entirely.
      return apply_update_result;
    }
  }

  return APPLY_UPDATE_SUCCESS;
}

// static
ApplyUpdateResult V4Store::AddUnlumpedHashes(PrefixSize prefix_size,
                                             const std::string& raw_hashes,
                                             HashPrefixMap* additions_map) {
  return AddUnlumpedHashes(prefix_size, raw_hashes.data(), raw_hashes.size(),
                           additions_map);
}

// static
ApplyUpdateResult V4Store::AddUnlumpedHashes(PrefixSize prefix_size,
                                             const char* raw_hashes_begin,
                                             const size_t raw_hashes_length,
                                             HashPrefixMap* additions_map) {
  if (prefix_size < kMinHashPrefixLength) {
    NOTREACHED();
    return PREFIX_SIZE_TOO_SMALL_FAILURE;
  }
  if (prefix_size > kMaxHashPrefixLength) {
    NOTREACHED();
    return PREFIX_SIZE_TOO_LARGE_FAILURE;
  }
  if (raw_hashes_length % prefix_size != 0) {
    return ADDITIONS_SIZE_UNEXPECTED_FAILURE;
  }

  TimeTicks before = TimeTicks::Now();
  // TODO(vakh): Figure out a way to avoid the following copy operation.
  (*additions_map)[prefix_size] =
      std::string(raw_hashes_begin, raw_hashes_begin + raw_hashes_length);
  RecordAddUnlumpedHashesTime(TimeTicks::Now() - before);
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

ApplyUpdateResult V4Store::MergeUpdate(const HashPrefixMap& old_prefixes_map,
                                       const HashPrefixMap& additions_map,
                                       const RepeatedField<int32>* raw_removals,
                                       const std::string& expected_checksum) {
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

  bool calculate_checksum = !expected_checksum.empty();
  std::unique_ptr<crypto::SecureHash> checksum_ctx(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));

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

        if (calculate_checksum) {
          checksum_ctx->Update(base::string_as_array(&next_smallest_prefix_old),
                               next_smallest_prefix_size);
        }
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

      if (calculate_checksum) {
        checksum_ctx->Update(
            base::string_as_array(&next_smallest_prefix_additions),
            next_smallest_prefix_size);
      }

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

  if (raw_removals && removals_iter != raw_removals->end()) {
    return REMOVALS_INDEX_TOO_LARGE_FAILURE;
  }

  if (calculate_checksum) {
    std::string checksum(crypto::kSHA256Length, 0);
    checksum_ctx->Finish(base::string_as_array(&checksum), checksum.size());
    if (checksum != expected_checksum) {
      std::string checksum_base64, expected_checksum_base64;
      base::Base64Encode(checksum, &checksum_base64);
      base::Base64Encode(expected_checksum, &expected_checksum_base64);
      DVLOG(1) << "Failure: Checksum mismatch: calculated: " << checksum_base64
               << " expected: " << expected_checksum_base64;
      return CHECKSUM_MISMATCH_FAILURE;
    }
  }

  return APPLY_UPDATE_SUCCESS;
}

StoreReadResult V4Store::ReadFromDisk() {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  TimeTicks before = TimeTicks::Now();
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
    return UNEXPECTED_MAGIC_NUMBER_FAILURE;
  }

  UMA_HISTOGRAM_SPARSE_SLOWLY("SafeBrowsing.V4StoreVersionRead",
                              file_format.version_number());
  if (file_format.version_number() != kFileVersion) {
    return FILE_VERSION_INCOMPATIBLE_FAILURE;
  }

  if (!file_format.has_list_update_response()) {
    return HASH_PREFIX_INFO_MISSING_FAILURE;
  }

  std::unique_ptr<ListUpdateResponse> response(new ListUpdateResponse);
  response->Swap(file_format.mutable_list_update_response());
  ApplyUpdateResult apply_update_result = ProcessFullUpdate(response);
  RecordApplyUpdateResultWhenReadingFromDisk(apply_update_result);
  if (apply_update_result != APPLY_UPDATE_SUCCESS) {
    hash_prefix_map_.clear();
    return HASH_PREFIX_MAP_GENERATION_FAILURE;
  }
  RecordReadFromDiskTime(TimeTicks::Now() - before, store_path_);

  return READ_SUCCESS;
}

StoreWriteResult V4Store::WriteToDisk(
    std::unique_ptr<ListUpdateResponse> response) const {
  // Do not write partial updates to the disk.
  // After merging the updates, the ListUpdateResponse passed to this method
  // should be a FULL_UPDATE.
  if (!response->has_response_type() ||
      response->response_type() != ListUpdateResponse::FULL_UPDATE) {
    DVLOG(1) << "Failure: response->has_response_type(): "
             << response->has_response_type()
             << " : response->response_type(): " << response->response_type();
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
