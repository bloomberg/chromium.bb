// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/prefix_set.h"

#include <algorithm>
#include <math.h>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/md5.h"
#include "base/metrics/histogram.h"

namespace {

// |kMagic| should be reasonably unique, and not match itself across
// endianness changes.  I generated this value with:
// md5 -qs chrome/browser/safe_browsing/prefix_set.cc | colrm 9
static uint32 kMagic = 0x864088dd;

// Current version the code writes out.
static uint32 kVersion = 0x1;

typedef struct {
  uint32 magic;
  uint32 version;
  uint32 index_size;
  uint32 deltas_size;
} FileHeader;

// For |std::upper_bound()| to find a prefix w/in a vector of pairs.
bool PrefixLess(const std::pair<SBPrefix,size_t>& a,
                const std::pair<SBPrefix,size_t>& b) {
  return a.first < b.first;
}

}  // namespace

namespace safe_browsing {

PrefixSet::PrefixSet(const std::vector<SBPrefix>& sorted_prefixes) {
  if (sorted_prefixes.size()) {
    // Lead with the first prefix.
    SBPrefix prev_prefix = sorted_prefixes[0];
    size_t run_length = 0;
    index_.push_back(std::make_pair(prev_prefix, deltas_.size()));

    // Used to build a checksum from the data used to construct the
    // structures.  Since the data is a bunch of uniform hashes, it
    // seems reasonable to just xor most of it in, rather than trying
    // to use a more complicated algorithm.
    uint32 checksum = static_cast<uint32>(sorted_prefixes[0]);
    checksum ^= static_cast<uint32>(deltas_.size());

    for (size_t i = 1; i < sorted_prefixes.size(); ++i) {
      // Skip duplicates.
      if (sorted_prefixes[i] == prev_prefix)
        continue;

      // Calculate the delta.  |unsigned| is mandatory, because the
      // sorted_prefixes could be more than INT_MAX apart.
      DCHECK_GT(sorted_prefixes[i], prev_prefix);
      const unsigned delta = sorted_prefixes[i] - prev_prefix;
      const uint16 delta16 = static_cast<uint16>(delta);

      // New index ref if the delta doesn't fit, or if too many
      // consecutive deltas have been encoded.
      if (delta != static_cast<unsigned>(delta16) || run_length >= kMaxRun) {
        checksum ^= static_cast<uint32>(sorted_prefixes[i]);
        checksum ^= static_cast<uint32>(deltas_.size());
        index_.push_back(std::make_pair(sorted_prefixes[i], deltas_.size()));
        run_length = 0;
      } else {
        checksum ^= static_cast<uint32>(delta16);
        // Continue the run of deltas.
        deltas_.push_back(delta16);
        DCHECK_EQ(static_cast<unsigned>(deltas_.back()), delta);
        ++run_length;
      }

      prev_prefix = sorted_prefixes[i];
    }
    checksum_ = checksum;
    DCHECK(CheckChecksum());

    // Send up some memory-usage stats.  Bits because fractional bytes
    // are weird.
    const size_t bits_used = index_.size() * sizeof(index_[0]) * CHAR_BIT +
        deltas_.size() * sizeof(deltas_[0]) * CHAR_BIT;
    const size_t unique_prefixes = index_.size() + deltas_.size();
    static const size_t kMaxBitsPerPrefix = sizeof(SBPrefix) * CHAR_BIT;
    UMA_HISTOGRAM_ENUMERATION("SB2.PrefixSetBitsPerPrefix",
                              bits_used / unique_prefixes,
                              kMaxBitsPerPrefix);
  }
}

PrefixSet::PrefixSet(std::vector<std::pair<SBPrefix,size_t> > *index,
                     std::vector<uint16> *deltas) {
  DCHECK(index && deltas);
  index_.swap(*index);
  deltas_.swap(*deltas);
}

PrefixSet::~PrefixSet() {}

bool PrefixSet::Exists(SBPrefix prefix) const {
  if (index_.empty())
    return false;

  // Find the first position after |prefix| in |index_|.
  std::vector<std::pair<SBPrefix,size_t> >::const_iterator
      iter = std::upper_bound(index_.begin(), index_.end(),
                              std::pair<SBPrefix,size_t>(prefix, 0),
                              PrefixLess);

  // |prefix| comes before anything that's in the set.
  if (iter == index_.begin())
    return false;

  // Capture the upper bound of our target entry's deltas.
  const size_t bound = (iter == index_.end() ? deltas_.size() : iter->second);

  // Back up to the entry our target is in.
  --iter;

  // All prefixes in |index_| are in the set.
  SBPrefix current = iter->first;
  if (current == prefix)
    return true;

  // Scan forward accumulating deltas while a match is possible.
  for (size_t di = iter->second; di < bound && current < prefix; ++di) {
    current += deltas_[di];
  }

  return current == prefix;
}

void PrefixSet::GetPrefixes(std::vector<SBPrefix>* prefixes) const {
  for (size_t ii = 0; ii < index_.size(); ++ii) {
    // The deltas for this |index_| entry run to the next index entry,
    // or the end of the deltas.
    const size_t deltas_end =
        (ii + 1 < index_.size()) ? index_[ii + 1].second : deltas_.size();

    SBPrefix current = index_[ii].first;
    prefixes->push_back(current);
    for (size_t di = index_[ii].second; di < deltas_end; ++di) {
      current += deltas_[di];
      prefixes->push_back(current);
    }
  }
}

// static
PrefixSet* PrefixSet::LoadFile(const FilePath& filter_name) {
  int64 size_64;
  if (!file_util::GetFileSize(filter_name, &size_64))
    return NULL;
  if (size_64 < static_cast<int64>(sizeof(FileHeader) + sizeof(MD5Digest)))
    return NULL;

  file_util::ScopedFILE file(file_util::OpenFile(filter_name, "rb"));
  if (!file.get())
    return NULL;

  FileHeader header;
  size_t read = fread(&header, sizeof(header), 1, file.get());
  if (read != 1)
    return NULL;

  if (header.magic != kMagic || header.version != kVersion)
    return NULL;

  std::vector<std::pair<SBPrefix,size_t> > index;
  const size_t index_bytes = sizeof(index[0]) * header.index_size;

  std::vector<uint16> deltas;
  const size_t deltas_bytes = sizeof(deltas[0]) * header.deltas_size;

  // Check for bogus sizes before allocating any space.
  const size_t expected_bytes =
      sizeof(header) + index_bytes + deltas_bytes + sizeof(MD5Digest);
  if (static_cast<int64>(expected_bytes) != size_64)
    return NULL;

  // The file looks valid, start building the digest.
  MD5Context context;
  MD5Init(&context);
  MD5Update(&context, &header, sizeof(header));

  // Read the index vector.  Herb Sutter indicates that vectors are
  // guaranteed to be contiuguous, so reading to where element 0 lives
  // is valid.
  index.resize(header.index_size);
  read = fread(&(index[0]), sizeof(index[0]), index.size(), file.get());
  if (read != index.size())
    return NULL;
  MD5Update(&context, &(index[0]), index_bytes);

  // Read vector of deltas.
  deltas.resize(header.deltas_size);
  read = fread(&(deltas[0]), sizeof(deltas[0]), deltas.size(), file.get());
  if (read != deltas.size())
    return NULL;
  MD5Update(&context, &(deltas[0]), deltas_bytes);

  MD5Digest calculated_digest;
  MD5Final(&calculated_digest, &context);

  MD5Digest file_digest;
  read = fread(&file_digest, sizeof(file_digest), 1, file.get());
  if (read != 1)
    return NULL;

  if (0 != memcmp(&file_digest, &calculated_digest, sizeof(file_digest)))
    return NULL;

  // Steals contents of |index| and |deltas| via swap().
  return new PrefixSet(&index, &deltas);
}

bool PrefixSet::WriteFile(const FilePath& filter_name) const {
  FileHeader header;
  header.magic = kMagic;
  header.version = kVersion;
  header.index_size = static_cast<uint32>(index_.size());
  header.deltas_size = static_cast<uint32>(deltas_.size());

  // Sanity check that the 32-bit values never mess things up.
  if (static_cast<size_t>(header.index_size) != index_.size() ||
      static_cast<size_t>(header.deltas_size) != deltas_.size()) {
    NOTREACHED();
    return false;
  }

  file_util::ScopedFILE file(file_util::OpenFile(filter_name, "wb"));
  if (!file.get())
    return false;

  MD5Context context;
  MD5Init(&context);

  // TODO(shess): The I/O code in safe_browsing_store_file.cc would
  // sure be useful about now.
  size_t written = fwrite(&header, sizeof(header), 1, file.get());
  if (written != 1)
    return false;
  MD5Update(&context, &header, sizeof(header));

  // As for reads, the standard guarantees the ability to access the
  // contents of the vector by a pointer to an element.
  const size_t index_bytes = sizeof(index_[0]) * index_.size();
  written = fwrite(&(index_[0]), sizeof(index_[0]), index_.size(), file.get());
  if (written != index_.size())
    return false;
  MD5Update(&context, &(index_[0]), index_bytes);

  const size_t deltas_bytes = sizeof(deltas_[0]) * deltas_.size();
  written = fwrite(&(deltas_[0]), sizeof(deltas_[0]), deltas_.size(),
                   file.get());
  if (written != deltas_.size())
    return false;
  MD5Update(&context, &(deltas_[0]), deltas_bytes);

  MD5Digest digest;
  MD5Final(&digest, &context);
  written = fwrite(&digest, sizeof(digest), 1, file.get());
  if (written != 1)
    return false;

  // TODO(shess): Can this code check that the close was successful?
  file.reset();

  return true;
}

size_t PrefixSet::IndexBinFor(size_t target_index) const {
  // The items in |index_| have the logical index of each previous
  // item in |index_| plus the count of deltas between the items.
  // Since the indices into |deltas_| are absolute, the logical index
  // is then the sum of the two indices.
  size_t lo = 0;
  size_t hi = index_.size();

  // Binary search because linear search was too slow (really, the
  // unit test sucked).  Inline because the elements can't be compared
  // in isolation (their position matters).
  while (hi - lo > 1) {
    const size_t i = (lo + hi) / 2;

    if (target_index < i + index_[i].second) {
      DCHECK_LT(i, hi);  // Always making progress.
      hi = i;
    } else {
      DCHECK_GT(i, lo);  // Always making progress.
      lo = i;
    }
  }
  return lo;
}

size_t PrefixSet::GetSize() const {
  return index_.size() + deltas_.size();
}

bool PrefixSet::IsDeltaAt(size_t target_index) const {
  CHECK_LT(target_index, GetSize());

  const size_t i = IndexBinFor(target_index);
  return target_index > i + index_[i].second;
}

uint16 PrefixSet::DeltaAt(size_t target_index) const {
  CHECK_LT(target_index, GetSize());

  // Find the |index_| entry which contains |target_index|.
  const size_t i = IndexBinFor(target_index);

  // Exactly on the |index_| entry means no delta.
  CHECK_GT(target_index, i + index_[i].second);

  // -i backs out the |index_| entries, -1 gets the delta that lead to
  // the value at |target_index|.
  CHECK_LT(target_index - i - 1, deltas_.size());
  return deltas_[target_index - i - 1];
}

bool PrefixSet::CheckChecksum() const {
  uint32 checksum = 0;

  for (size_t ii = 0; ii < index_.size(); ++ii) {
    checksum ^= static_cast<uint32>(index_[ii].first);
    checksum ^= static_cast<uint32>(index_[ii].second);
  }

  for (size_t di = 0; di < deltas_.size(); ++di) {
    checksum ^= static_cast<uint32>(deltas_[di]);
  }

  return checksum == checksum_;
}

}  // namespace safe_browsing
