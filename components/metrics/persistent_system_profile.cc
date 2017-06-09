// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/persistent_system_profile.h"

#include "base/atomicops.h"
#include "base/memory/singleton.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/stl_util.h"

namespace metrics {

namespace {

// To provide atomic addition of records so that there is no confusion between
// writers and readers, all of the metadata about a record is contained in a
// structure that can be stored as a single atomic 32-bit word.
union RecordHeader {
  struct {
    unsigned continued : 1;  // Flag indicating if there is more after this.
    unsigned type : 7;       // The type of this record.
    unsigned amount : 24;    // The amount of data to follow.
  } as_parts;
  base::subtle::Atomic32 as_atomic;
};

constexpr uint32_t kTypeIdSystemProfile = 0x330A7150;  // SHA1(SystemProfile)
constexpr size_t kSystemProfileAllocSize = 4 << 10;    // 4 KiB
constexpr size_t kMaxRecordSize = (1 << 24) - sizeof(RecordHeader);

static_assert(sizeof(RecordHeader) == sizeof(base::subtle::Atomic32),
              "bad RecordHeader size");

// Calculate the size of a record based on the amount of data. This adds room
// for the record header and rounds up to the next multiple of the record-header
// size.
size_t CalculateRecordSize(size_t data_amount) {
  return (data_amount + sizeof(RecordHeader) + sizeof(RecordHeader) - 1) &
         ~(sizeof(RecordHeader) - 1);
}

}  // namespace

PersistentSystemProfile::RecordAllocator::RecordAllocator(
    base::PersistentMemoryAllocator* memory_allocator,
    size_t min_size)
    : allocator_(memory_allocator),
      alloc_reference_(0),
      alloc_size_(0),
      end_offset_(0) {
  AddSegment(min_size);
}

PersistentSystemProfile::RecordAllocator::RecordAllocator(
    const base::PersistentMemoryAllocator* memory_allocator)
    : allocator_(
          const_cast<base::PersistentMemoryAllocator*>(memory_allocator)),
      alloc_reference_(0),
      alloc_size_(0),
      end_offset_(0) {}

void PersistentSystemProfile::RecordAllocator::Reset() {
  // Clear the first word of all blocks so they're known to be "empty".
  alloc_reference_ = 0;
  while (NextSegment()) {
    // Get the block as a char* and cast it. It can't be fetched directly as
    // an array of RecordHeader because that's not a fundamental type and only
    // arrays of fundamental types are allowed.
    RecordHeader* header =
        reinterpret_cast<RecordHeader*>(allocator_->GetAsArray<char>(
            alloc_reference_, kTypeIdSystemProfile, sizeof(RecordHeader)));
    DCHECK(header);
    base::subtle::NoBarrier_Store(&header->as_atomic, 0);
  }

  // Reset member variables.
  alloc_reference_ = 0;
  alloc_size_ = 0;
  end_offset_ = 0;
}

bool PersistentSystemProfile::RecordAllocator::Write(
    RecordType type,
    const std::string& record) {
  const char* data = record.data();
  size_t remaining_size = record.size();

  // Allocate space and write records until everything has been stored.
  do {
    if (end_offset_ == alloc_size_) {
      if (!AddSegment(remaining_size))
        return false;
    }
    // Write out as much of the data as possible. |data| and |remaining_size|
    // are updated in place.
    if (!WriteData(type, &data, &remaining_size))
      return false;
  } while (remaining_size > 0);

  return true;
}

bool PersistentSystemProfile::RecordAllocator::HasMoreData() const {
  if (alloc_reference_ == 0 && !NextSegment())
    return false;

  char* block =
      allocator_->GetAsArray<char>(alloc_reference_, kTypeIdSystemProfile,
                                   base::PersistentMemoryAllocator::kSizeAny);
  if (!block)
    return false;

  RecordHeader header;
  header.as_atomic = base::subtle::Acquire_Load(
      reinterpret_cast<base::subtle::Atomic32*>(block + end_offset_));
  return header.as_parts.type != kUnusedSpace;
}

bool PersistentSystemProfile::RecordAllocator::Read(RecordType* type,
                                                    std::string* record) const {
  *type = kUnusedSpace;
  record->clear();

  // Access data and read records until everything has been loaded.
  while (true) {
    if (end_offset_ == alloc_size_) {
      if (!NextSegment())
        return false;
    }
    if (ReadData(type, record))
      return *type != kUnusedSpace;
  }
}

bool PersistentSystemProfile::RecordAllocator::NextSegment() const {
  base::PersistentMemoryAllocator::Iterator iter(allocator_, alloc_reference_);
  alloc_reference_ = iter.GetNextOfType(kTypeIdSystemProfile);
  alloc_size_ = allocator_->GetAllocSize(alloc_reference_);
  end_offset_ = 0;
  return alloc_reference_ != 0;
}

bool PersistentSystemProfile::RecordAllocator::AddSegment(size_t min_size) {
  if (NextSegment()) {
    // The first record-header should have been zeroed as part of the allocation
    // or by the "reset" procedure.
    DCHECK_EQ(0, base::subtle::NoBarrier_Load(
                     allocator_->GetAsArray<base::subtle::Atomic32>(
                         alloc_reference_, kTypeIdSystemProfile, 1)));
    return true;
  }

  DCHECK_EQ(0U, alloc_reference_);
  DCHECK_EQ(0U, end_offset_);

  size_t size =
      std::max(CalculateRecordSize(min_size), kSystemProfileAllocSize);

  uint32_t ref = allocator_->Allocate(size, kTypeIdSystemProfile);
  if (!ref)
    return false;  // Allocator must be full.
  allocator_->MakeIterable(ref);

  alloc_reference_ = ref;
  alloc_size_ = allocator_->GetAllocSize(ref);
  return true;
}

bool PersistentSystemProfile::RecordAllocator::WriteData(
    RecordType type,
    const char** data,
    size_t* remaining_size) {
  char* block =
      allocator_->GetAsArray<char>(alloc_reference_, kTypeIdSystemProfile,
                                   base::PersistentMemoryAllocator::kSizeAny);
  if (!block)
    return false;  // It's bad if there is no accessible block.

  size_t write_size = std::min(*remaining_size, kMaxRecordSize);
  write_size =
      std::min(write_size, alloc_size_ - end_offset_ - sizeof(RecordHeader));

  // Write the data and the record header.
  RecordHeader header;
  header.as_atomic = 0;
  header.as_parts.type = type;
  header.as_parts.amount = write_size;
  header.as_parts.continued = (write_size < *remaining_size);
  size_t offset = end_offset_;
  end_offset_ += CalculateRecordSize(write_size);
  DCHECK_GE(alloc_size_, end_offset_);
  if (end_offset_ < alloc_size_) {
    // An empty record header has to be next before this one gets written.
    base::subtle::NoBarrier_Store(
        reinterpret_cast<base::subtle::Atomic32*>(block + end_offset_), 0);
  }
  memcpy(block + offset + sizeof(header), *data, write_size);
  base::subtle::Release_Store(reinterpret_cast<base::subtle::Atomic32*>(block),
                              header.as_atomic);

  // Account for what was stored and prepare for follow-on records with any
  // remaining data.
  *data += write_size;
  *remaining_size -= write_size;

  return true;
}

bool PersistentSystemProfile::RecordAllocator::ReadData(
    RecordType* type,
    std::string* record) const {
  DCHECK_GT(alloc_size_, end_offset_);

  char* block =
      allocator_->GetAsArray<char>(alloc_reference_, kTypeIdSystemProfile,
                                   base::PersistentMemoryAllocator::kSizeAny);
  if (!block) {
    *type = kUnusedSpace;
    return true;  // No more data.
  }

  // Get and validate the record header.
  RecordHeader header;
  header.as_atomic = base::subtle::Acquire_Load(
      reinterpret_cast<base::subtle::Atomic32*>(block + end_offset_));
  bool continued = !!header.as_parts.continued;
  if (header.as_parts.type == kUnusedSpace) {
    *type = kUnusedSpace;
    return true;  // End of all records.
  } else if (*type == kUnusedSpace) {
    *type = static_cast<RecordType>(header.as_parts.type);
  } else if (*type != header.as_parts.type) {
    NOTREACHED();  // Continuation didn't match start of record.
    *type = kUnusedSpace;
    record->clear();
    return false;
  }
  size_t read_size = header.as_parts.amount;
  if (read_size < sizeof(header) ||
      end_offset_ + sizeof(header) + read_size > alloc_size_) {
    NOTREACHED();  // Invalid header amount.
    *type = kUnusedSpace;
    return true;  // Don't try again.
  }

  // Append the record data to the output string.
  record->append(block + sizeof(header), read_size);
  end_offset_ += CalculateRecordSize(read_size);
  DCHECK_GE(alloc_size_, end_offset_);

  return !continued;
}

PersistentSystemProfile::PersistentSystemProfile() {}

PersistentSystemProfile::~PersistentSystemProfile() {}

void PersistentSystemProfile::RegisterPersistentAllocator(
    base::PersistentMemoryAllocator* memory_allocator) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Create and store the allocator. A |min_size| of "1" ensures that a memory
  // block is reserved now.
  RecordAllocator allocator(memory_allocator, 1);
  allocators_.push_back(std::move(allocator));
}

void PersistentSystemProfile::DeregisterPersistentAllocator(
    base::PersistentMemoryAllocator* memory_allocator) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // This would be more efficient with a std::map but it's not expected that
  // allocators will get deregistered with any frequency, if at all.
  base::EraseIf(allocators_, [=](RecordAllocator& records) {
    return records.allocator() == memory_allocator;
  });
}

void PersistentSystemProfile::SetSystemProfile(
    const std::string& serialized_profile) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (allocators_.empty() || serialized_profile.empty())
    return;

  for (auto& allocator : allocators_) {
    // A full system profile always starts fresh.
    allocator.Reset();
    // Write out the serialized profile.
    allocator.Write(kSystemProfileProto, serialized_profile);
  }
}

void PersistentSystemProfile::SetSystemProfile(
    const SystemProfileProto& profile) {
  std::string serialized_profile;
  if (!profile.SerializeToString(&serialized_profile))
    return;
  SetSystemProfile(serialized_profile);
}

// static
bool PersistentSystemProfile::HasSystemProfile(
    const base::PersistentMemoryAllocator& memory_allocator) {
  const RecordAllocator records(&memory_allocator);
  return records.HasMoreData();
}

// static
bool PersistentSystemProfile::GetSystemProfile(
    const base::PersistentMemoryAllocator& memory_allocator,
    SystemProfileProto* system_profile) {
  const RecordAllocator records(&memory_allocator);

  RecordType type;
  std::string record;
  if (!records.Read(&type, &record))
    return false;
  if (type != kSystemProfileProto)
    return false;

  return system_profile->ParseFromString(record);
}

GlobalPersistentSystemProfile* GlobalPersistentSystemProfile::GetInstance() {
  return base::Singleton<
      GlobalPersistentSystemProfile,
      base::LeakySingletonTraits<GlobalPersistentSystemProfile>>::get();
}

}  // namespace metrics
