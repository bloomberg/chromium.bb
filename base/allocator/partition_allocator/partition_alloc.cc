// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_alloc.h"

#include <string.h>
#include <type_traits>

#include "base/allocator/partition_allocator/oom.h"
#include "base/allocator/partition_allocator/spin_lock.h"
#include "base/compiler_specific.h"
#include "base/lazy_instance.h"

// Two partition pages are used as guard / metadata page so make sure the super
// page size is bigger.
static_assert(base::kPartitionPageSize * 4 <= base::kSuperPageSize,
              "ok super page size");
static_assert(!(base::kSuperPageSize % base::kPartitionPageSize),
              "ok super page multiple");
// Four system pages gives us room to hack out a still-guard-paged piece
// of metadata in the middle of a guard partition page.
static_assert(base::kSystemPageSize * 4 <= base::kPartitionPageSize,
              "ok partition page size");
static_assert(!(base::kPartitionPageSize % base::kSystemPageSize),
              "ok partition page multiple");
static_assert(sizeof(base::PartitionPage) <= base::kPageMetadataSize,
              "PartitionPage should not be too big");
static_assert(sizeof(base::PartitionBucket) <= base::kPageMetadataSize,
              "PartitionBucket should not be too big");
static_assert(sizeof(base::PartitionSuperPageExtentEntry) <=
                  base::kPageMetadataSize,
              "PartitionSuperPageExtentEntry should not be too big");
static_assert(base::kPageMetadataSize * base::kNumPartitionPagesPerSuperPage <=
                  base::kSystemPageSize,
              "page metadata fits in hole");
// Limit to prevent callers accidentally overflowing an int size.
static_assert(base::kGenericMaxDirectMapped <= 1UL << 31,
              "maximum direct mapped allocation");
// Check that some of our zanier calculations worked out as expected.
static_assert(base::kGenericSmallestBucket == 8, "generic smallest bucket");
static_assert(base::kGenericMaxBucketed == 983040, "generic max bucketed");
static_assert(base::kMaxSystemPagesPerSlotSpan < (1 << 8),
              "System pages per slot span must be less than 128.");

namespace base {

namespace {

// g_sentinel_page is used as a sentinel to indicate that there is no page
// in the active page list. We can use nullptr, but in that case we need
// to add a null-check branch to the hot allocation path. We want to avoid
// that.
PartitionPage g_sentinel_page;
PartitionBucket g_sentinel_bucket;

}  // namespace

PartitionRootBase::PartitionRootBase() = default;
PartitionRootBase::~PartitionRootBase() = default;
PartitionRoot::PartitionRoot() = default;
PartitionRoot::~PartitionRoot() = default;
PartitionRootGeneric::PartitionRootGeneric() = default;
PartitionRootGeneric::~PartitionRootGeneric() = default;
PartitionAllocatorGeneric::PartitionAllocatorGeneric() = default;
PartitionAllocatorGeneric::~PartitionAllocatorGeneric() = default;

static LazyInstance<subtle::SpinLock>::Leaky g_initialized_lock =
    LAZY_INSTANCE_INITIALIZER;
static bool g_initialized = false;

void (*PartitionRootBase::gOomHandlingFunction)() = nullptr;
PartitionAllocHooks::AllocationHook* PartitionAllocHooks::allocation_hook_ =
    nullptr;
PartitionAllocHooks::FreeHook* PartitionAllocHooks::free_hook_ = nullptr;

// TODO(ajwong): This seems to interact badly with
// get_pages_per_slot_span() which rounds the value from this up to a
// multiple of kNumSystemPagesPerPartitionPage (aka 4) anyways.
// http://crbug.com/776537
//
// TODO(ajwong): The waste calculation seems wrong. The PTE usage should cover
// both used and unsed pages.
// http://crbug.com/776537
uint8_t PartitionBucket::get_system_pages_per_slot_span() {
  // This works out reasonably for the current bucket sizes of the generic
  // allocator, and the current values of partition page size and constants.
  // Specifically, we have enough room to always pack the slots perfectly into
  // some number of system pages. The only waste is the waste associated with
  // unfaulted pages (i.e. wasted address space).
  // TODO: we end up using a lot of system pages for very small sizes. For
  // example, we'll use 12 system pages for slot size 24. The slot size is
  // so small that the waste would be tiny with just 4, or 1, system pages.
  // Later, we can investigate whether there are anti-fragmentation benefits
  // to using fewer system pages.
  double best_waste_ratio = 1.0f;
  uint16_t best_pages = 0;
  if (this->slot_size > kMaxSystemPagesPerSlotSpan * kSystemPageSize) {
    // TODO(ajwong): Why is there a DCHECK here for this?
    // http://crbug.com/776537
    DCHECK(!(this->slot_size % kSystemPageSize));
    best_pages = static_cast<uint16_t>(this->slot_size / kSystemPageSize);
    // TODO(ajwong): Should this be checking against
    // kMaxSystemPagesPerSlotSpan or numeric_limits<uint8_t>::max?
    // http://crbug.com/776537
    CHECK(best_pages < (1 << 8));
    return static_cast<uint8_t>(best_pages);
  }
  DCHECK(this->slot_size <= kMaxSystemPagesPerSlotSpan * kSystemPageSize);
  for (uint16_t i = kNumSystemPagesPerPartitionPage - 1;
       i <= kMaxSystemPagesPerSlotSpan; ++i) {
    size_t page_size = kSystemPageSize * i;
    size_t num_slots = page_size / this->slot_size;
    size_t waste = page_size - (num_slots * this->slot_size);
    // Leaving a page unfaulted is not free; the page will occupy an empty page
    // table entry.  Make a simple attempt to account for that.
    //
    // TODO(ajwong): This looks wrong. PTEs are allocated for all pages
    // regardless of whether or not they are wasted. Should it just
    // be waste += i * sizeof(void*)?
    // http://crbug.com/776537
    size_t num_remainder_pages = i & (kNumSystemPagesPerPartitionPage - 1);
    size_t num_unfaulted_pages =
        num_remainder_pages
            ? (kNumSystemPagesPerPartitionPage - num_remainder_pages)
            : 0;
    waste += sizeof(void*) * num_unfaulted_pages;
    double waste_ratio = (double)waste / (double)page_size;
    if (waste_ratio < best_waste_ratio) {
      best_waste_ratio = waste_ratio;
      best_pages = i;
    }
  }
  DCHECK(best_pages > 0);
  CHECK(best_pages <= kMaxSystemPagesPerSlotSpan);
  return static_cast<uint8_t>(best_pages);
}

static void PartitionAllocBaseInit(PartitionRootBase* root) {
  DCHECK(!root->initialized);
  {
    subtle::SpinLock::Guard guard(g_initialized_lock.Get());
    if (!g_initialized) {
      g_initialized = true;
      // We mark the sentinel bucket/page as free to make sure it is skipped by
      // our logic to find a new active page.
      g_sentinel_bucket.active_pages_head = &g_sentinel_page;
    }
  }

  root->initialized = true;

  // This is a "magic" value so we can test if a root pointer is valid.
  root->inverted_self = ~reinterpret_cast<uintptr_t>(root);
}

void PartitionBucket::Init(uint32_t new_slot_size) {
  slot_size = new_slot_size;
  active_pages_head = &g_sentinel_page;
  empty_pages_head = nullptr;
  decommitted_pages_head = nullptr;
  num_full_pages = 0;
  num_system_pages_per_slot_span = get_system_pages_per_slot_span();
}

void PartitionAllocGlobalInit(void (*oom_handling_function)()) {
  DCHECK(oom_handling_function);
  PartitionRootBase::gOomHandlingFunction = oom_handling_function;
}

void PartitionRoot::Init(size_t num_buckets, size_t max_allocation) {
  PartitionAllocBaseInit(this);

  this->num_buckets = num_buckets;
  this->max_allocation = max_allocation;
  size_t i;
  for (i = 0; i < this->num_buckets; ++i) {
    PartitionBucket* bucket = &this->buckets()[i];
    if (!i)
      bucket->Init(kAllocationGranularity);
    else
      bucket->Init(i << kBucketShift);
  }
}

void PartitionRootGeneric::Init() {
  subtle::SpinLock::Guard guard(this->lock);

  PartitionAllocBaseInit(this);

  // Precalculate some shift and mask constants used in the hot path.
  // Example: malloc(41) == 101001 binary.
  // Order is 6 (1 << 6-1) == 32 is highest bit set.
  // order_index is the next three MSB == 010 == 2.
  // sub_order_index_mask is a mask for the remaining bits == 11 (masking to 01
  // for
  // the sub_order_index).
  size_t order;
  for (order = 0; order <= kBitsPerSizeT; ++order) {
    size_t order_index_shift;
    if (order < kGenericNumBucketsPerOrderBits + 1)
      order_index_shift = 0;
    else
      order_index_shift = order - (kGenericNumBucketsPerOrderBits + 1);
    this->order_index_shifts[order] = order_index_shift;
    size_t sub_order_index_mask;
    if (order == kBitsPerSizeT) {
      // This avoids invoking undefined behavior for an excessive shift.
      sub_order_index_mask =
          static_cast<size_t>(-1) >> (kGenericNumBucketsPerOrderBits + 1);
    } else {
      sub_order_index_mask = ((static_cast<size_t>(1) << order) - 1) >>
                             (kGenericNumBucketsPerOrderBits + 1);
    }
    this->order_sub_index_masks[order] = sub_order_index_mask;
  }

  // Set up the actual usable buckets first.
  // Note that typical values (i.e. min allocation size of 8) will result in
  // pseudo buckets (size==9 etc. or more generally, size is not a multiple
  // of the smallest allocation granularity).
  // We avoid them in the bucket lookup map, but we tolerate them to keep the
  // code simpler and the structures more generic.
  size_t i, j;
  size_t current_size = kGenericSmallestBucket;
  size_t currentIncrement =
      kGenericSmallestBucket >> kGenericNumBucketsPerOrderBits;
  PartitionBucket* bucket = &this->buckets[0];
  for (i = 0; i < kGenericNumBucketedOrders; ++i) {
    for (j = 0; j < kGenericNumBucketsPerOrder; ++j) {
      bucket->Init(current_size);
      // Disable psuedo buckets so that touching them faults.
      if (current_size % kGenericSmallestBucket)
        bucket->active_pages_head = nullptr;
      current_size += currentIncrement;
      ++bucket;
    }
    currentIncrement <<= 1;
  }
  DCHECK(current_size == 1 << kGenericMaxBucketedOrder);
  DCHECK(bucket == &this->buckets[0] + kGenericNumBuckets);

  // Then set up the fast size -> bucket lookup table.
  bucket = &this->buckets[0];
  PartitionBucket** bucketPtr = &this->bucket_lookups[0];
  for (order = 0; order <= kBitsPerSizeT; ++order) {
    for (j = 0; j < kGenericNumBucketsPerOrder; ++j) {
      if (order < kGenericMinBucketedOrder) {
        // Use the bucket of the finest granularity for malloc(0) etc.
        *bucketPtr++ = &this->buckets[0];
      } else if (order > kGenericMaxBucketedOrder) {
        *bucketPtr++ = PartitionBucket::get_sentinel_bucket();
      } else {
        PartitionBucket* validBucket = bucket;
        // Skip over invalid buckets.
        while (validBucket->slot_size % kGenericSmallestBucket)
          validBucket++;
        *bucketPtr++ = validBucket;
        bucket++;
      }
    }
  }
  DCHECK(bucket == &this->buckets[0] + kGenericNumBuckets);
  DCHECK(bucketPtr == &this->bucket_lookups[0] +
                          ((kBitsPerSizeT + 1) * kGenericNumBucketsPerOrder));
  // And there's one last bucket lookup that will be hit for e.g. malloc(-1),
  // which tries to overflow to a non-existant order.
  *bucketPtr = PartitionBucket::get_sentinel_bucket();
}

#if !defined(ARCH_CPU_64_BITS)
static NOINLINE void PartitionOutOfMemoryWithLotsOfUncommitedPages() {
  OOM_CRASH();
}
#endif

static NOINLINE void PartitionOutOfMemory(const PartitionRootBase* root) {
#if !defined(ARCH_CPU_64_BITS)
  // Check whether this OOM is due to a lot of super pages that are allocated
  // but not committed, probably due to http://crbug.com/421387.
  if (root->total_size_of_super_pages +
          root->total_size_of_direct_mapped_pages -
          root->total_size_of_committed_pages >
      kReasonableSizeOfUnusedPages) {
    PartitionOutOfMemoryWithLotsOfUncommitedPages();
  }
#endif
  if (PartitionRootBase::gOomHandlingFunction)
    (*PartitionRootBase::gOomHandlingFunction)();
  OOM_CRASH();
}

static NOINLINE void PartitionExcessiveAllocationSize() {
  OOM_CRASH();
}

NOINLINE void PartitionBucket::OnFull() {
  OOM_CRASH();
}

ALWAYS_INLINE bool PartitionPage::is_active() const {
  DCHECK(this != &g_sentinel_page);
  DCHECK(!page_offset);
  return (num_allocated_slots > 0 &&
          (freelist_head || num_unprovisioned_slots));
}

ALWAYS_INLINE bool PartitionPage::is_full() const {
  DCHECK(this != &g_sentinel_page);
  DCHECK(!page_offset);
  bool ret = (num_allocated_slots == bucket->get_slots_per_span());
  if (ret) {
    DCHECK(!freelist_head);
    DCHECK(!num_unprovisioned_slots);
  }
  return ret;
}

ALWAYS_INLINE bool PartitionPage::is_empty() const {
  DCHECK(this != &g_sentinel_page);
  DCHECK(!page_offset);
  return (!num_allocated_slots && freelist_head);
}

ALWAYS_INLINE bool PartitionPage::is_decommitted() const {
  DCHECK(this != &g_sentinel_page);
  DCHECK(!page_offset);
  bool ret = (!num_allocated_slots && !freelist_head);
  if (ret) {
    DCHECK(!num_unprovisioned_slots);
    DCHECK(empty_cache_index == -1);
  }
  return ret;
}

static void PartitionIncreaseCommittedPages(PartitionRootBase* root,
                                            size_t len) {
  root->total_size_of_committed_pages += len;
  DCHECK(root->total_size_of_committed_pages <=
         root->total_size_of_super_pages +
             root->total_size_of_direct_mapped_pages);
}

static void PartitionDecreaseCommittedPages(PartitionRootBase* root,
                                            size_t len) {
  root->total_size_of_committed_pages -= len;
  DCHECK(root->total_size_of_committed_pages <=
         root->total_size_of_super_pages +
             root->total_size_of_direct_mapped_pages);
}

static ALWAYS_INLINE void PartitionDecommitSystemPages(PartitionRootBase* root,
                                                       void* address,
                                                       size_t length) {
  DecommitSystemPages(address, length);
  PartitionDecreaseCommittedPages(root, length);
}

static ALWAYS_INLINE void PartitionRecommitSystemPages(PartitionRootBase* root,
                                                       void* address,
                                                       size_t length) {
  CHECK(RecommitSystemPages(address, length, PageReadWrite));
  PartitionIncreaseCommittedPages(root, length);
}

ALWAYS_INLINE void* PartitionBucket::AllocNewSlotSpan(
    PartitionRootBase* root,
    int flags,
    uint16_t num_partition_pages) {
  DCHECK(!(reinterpret_cast<uintptr_t>(root->next_partition_page) %
           kPartitionPageSize));
  DCHECK(!(reinterpret_cast<uintptr_t>(root->next_partition_page_end) %
           kPartitionPageSize));
  DCHECK(num_partition_pages <= kNumPartitionPagesPerSuperPage);
  size_t total_size = kPartitionPageSize * num_partition_pages;
  size_t num_partition_pages_left =
      (root->next_partition_page_end - root->next_partition_page) >>
      kPartitionPageShift;
  if (LIKELY(num_partition_pages_left >= num_partition_pages)) {
    // In this case, we can still hand out pages from the current super page
    // allocation.
    char* ret = root->next_partition_page;

    // Fresh System Pages in the SuperPages are decommited. Commit them
    // before vending them back.
    CHECK(SetSystemPagesAccess(ret, total_size, PageReadWrite));

    root->next_partition_page += total_size;
    PartitionIncreaseCommittedPages(root, total_size);
    return ret;
  }

  // Need a new super page. We want to allocate super pages in a continguous
  // address region as much as possible. This is important for not causing
  // page table bloat and not fragmenting address spaces in 32 bit
  // architectures.
  char* requestedAddress = root->next_super_page;
  char* super_page = reinterpret_cast<char*>(AllocPages(
      requestedAddress, kSuperPageSize, kSuperPageSize, PageReadWrite));
  if (UNLIKELY(!super_page))
    return nullptr;

  root->total_size_of_super_pages += kSuperPageSize;
  PartitionIncreaseCommittedPages(root, total_size);

  // |total_size| MUST be less than kSuperPageSize - (kPartitionPageSize*2).
  // This is a trustworthy value because num_partition_pages is not user
  // controlled.
  //
  // TODO(ajwong): Introduce a DCHECK.
  root->next_super_page = super_page + kSuperPageSize;
  char* ret = super_page + kPartitionPageSize;
  root->next_partition_page = ret + total_size;
  root->next_partition_page_end = root->next_super_page - kPartitionPageSize;
  // Make the first partition page in the super page a guard page, but leave a
  // hole in the middle.
  // This is where we put page metadata and also a tiny amount of extent
  // metadata.
  CHECK(SetSystemPagesAccess(super_page, kSystemPageSize, PageInaccessible));
  CHECK(SetSystemPagesAccess(super_page + (kSystemPageSize * 2),
                             kPartitionPageSize - (kSystemPageSize * 2),
                             PageInaccessible));
  //  CHECK(SetSystemPagesAccess(super_page + (kSuperPageSize -
  //  kPartitionPageSize),
  //                             kPartitionPageSize, PageInaccessible));
  // All remaining slotspans for the unallocated PartitionPages inside the
  // SuperPage are conceptually decommitted. Correctly set the state here
  // so they do not occupy resources.
  //
  // TODO(ajwong): Refactor Page Allocator API so the SuperPage comes in
  // decommited initially.
  CHECK(SetSystemPagesAccess(super_page + kPartitionPageSize + total_size,
                             (kSuperPageSize - kPartitionPageSize - total_size),
                             PageInaccessible));

  // If we were after a specific address, but didn't get it, assume that
  // the system chose a lousy address. Here most OS'es have a default
  // algorithm that isn't randomized. For example, most Linux
  // distributions will allocate the mapping directly before the last
  // successful mapping, which is far from random. So we just get fresh
  // randomness for the next mapping attempt.
  if (requestedAddress && requestedAddress != super_page)
    root->next_super_page = nullptr;

  // We allocated a new super page so update super page metadata.
  // First check if this is a new extent or not.
  PartitionSuperPageExtentEntry* latest_extent =
      reinterpret_cast<PartitionSuperPageExtentEntry*>(
          PartitionSuperPageToMetadataArea(super_page));
  // By storing the root in every extent metadata object, we have a fast way
  // to go from a pointer within the partition to the root object.
  latest_extent->root = root;
  // Most new extents will be part of a larger extent, and these three fields
  // are unused, but we initialize them to 0 so that we get a clear signal
  // in case they are accidentally used.
  latest_extent->super_page_base = nullptr;
  latest_extent->super_pages_end = nullptr;
  latest_extent->next = nullptr;

  PartitionSuperPageExtentEntry* current_extent = root->current_extent;
  bool isNewExtent = (super_page != requestedAddress);
  if (UNLIKELY(isNewExtent)) {
    if (UNLIKELY(!current_extent)) {
      DCHECK(!root->first_extent);
      root->first_extent = latest_extent;
    } else {
      DCHECK(current_extent->super_page_base);
      current_extent->next = latest_extent;
    }
    root->current_extent = latest_extent;
    latest_extent->super_page_base = super_page;
    latest_extent->super_pages_end = super_page + kSuperPageSize;
  } else {
    // We allocated next to an existing extent so just nudge the size up a
    // little.
    DCHECK(current_extent->super_pages_end);
    current_extent->super_pages_end += kSuperPageSize;
    DCHECK(ret >= current_extent->super_page_base &&
           ret < current_extent->super_pages_end);
  }
  return ret;
}

ALWAYS_INLINE uint16_t PartitionBucket::get_pages_per_slot_span() {
  // Rounds up to nearest multiple of kNumSystemPagesPerPartitionPage.
  return (num_system_pages_per_slot_span +
          (kNumSystemPagesPerPartitionPage - 1)) /
         kNumSystemPagesPerPartitionPage;
}

ALWAYS_INLINE void PartitionPage::Reset() {
  DCHECK(this->is_decommitted());

  num_unprovisioned_slots = bucket->get_slots_per_span();
  DCHECK(num_unprovisioned_slots);

  next_page = nullptr;
}

ALWAYS_INLINE void PartitionBucket::InitializeSlotSpan(PartitionPage* page) {
  // The bucket never changes. We set it up once.
  page->bucket = this;
  page->empty_cache_index = -1;

  page->Reset();

  // If this page has just a single slot, do not set up page offsets for any
  // page metadata other than the first one. This ensures that attempts to
  // touch invalid page metadata fail.
  if (page->num_unprovisioned_slots == 1)
    return;

  uint16_t num_partition_pages = get_pages_per_slot_span();
  char* page_char_ptr = reinterpret_cast<char*>(page);
  for (uint16_t i = 1; i < num_partition_pages; ++i) {
    page_char_ptr += kPageMetadataSize;
    PartitionPage* secondary_page =
        reinterpret_cast<PartitionPage*>(page_char_ptr);
    secondary_page->page_offset = i;
  }
}

ALWAYS_INLINE char* PartitionBucket::AllocAndFillFreelist(PartitionPage* page) {
  DCHECK(page != PartitionPage::get_sentinel_page());
  uint16_t num_slots = page->num_unprovisioned_slots;
  DCHECK(num_slots);
  // We should only get here when _every_ slot is either used or unprovisioned.
  // (The third state is "on the freelist". If we have a non-empty freelist, we
  // should not get here.)
  DCHECK(num_slots + page->num_allocated_slots == this->get_slots_per_span());
  // Similarly, make explicitly sure that the freelist is empty.
  DCHECK(!page->freelist_head);
  DCHECK(page->num_allocated_slots >= 0);

  size_t size = this->slot_size;
  char* base = reinterpret_cast<char*>(PartitionPage::ToPointer(page));
  char* return_object = base + (size * page->num_allocated_slots);
  char* firstFreelistPointer = return_object + size;
  char* firstFreelistPointerExtent =
      firstFreelistPointer + sizeof(PartitionFreelistEntry*);
  // Our goal is to fault as few system pages as possible. We calculate the
  // page containing the "end" of the returned slot, and then allow freelist
  // pointers to be written up to the end of that page.
  char* sub_page_limit = reinterpret_cast<char*>(
      RoundUpToSystemPage(reinterpret_cast<size_t>(firstFreelistPointer)));
  char* slots_limit = return_object + (size * num_slots);
  char* freelist_limit = sub_page_limit;
  if (UNLIKELY(slots_limit < freelist_limit))
    freelist_limit = slots_limit;

  uint16_t num_new_freelist_entries = 0;
  if (LIKELY(firstFreelistPointerExtent <= freelist_limit)) {
    // Only consider used space in the slot span. If we consider wasted
    // space, we may get an off-by-one when a freelist pointer fits in the
    // wasted space, but a slot does not.
    // We know we can fit at least one freelist pointer.
    num_new_freelist_entries = 1;
    // Any further entries require space for the whole slot span.
    num_new_freelist_entries += static_cast<uint16_t>(
        (freelist_limit - firstFreelistPointerExtent) / size);
  }

  // We always return an object slot -- that's the +1 below.
  // We do not neccessarily create any new freelist entries, because we cross
  // sub page boundaries frequently for large bucket sizes.
  DCHECK(num_new_freelist_entries + 1 <= num_slots);
  num_slots -= (num_new_freelist_entries + 1);
  page->num_unprovisioned_slots = num_slots;
  page->num_allocated_slots++;

  if (LIKELY(num_new_freelist_entries)) {
    char* freelist_pointer = firstFreelistPointer;
    PartitionFreelistEntry* entry =
        reinterpret_cast<PartitionFreelistEntry*>(freelist_pointer);
    page->freelist_head = entry;
    while (--num_new_freelist_entries) {
      freelist_pointer += size;
      PartitionFreelistEntry* next_entry =
          reinterpret_cast<PartitionFreelistEntry*>(freelist_pointer);
      entry->next = PartitionFreelistEntry::Transform(next_entry);
      entry = next_entry;
    }
    entry->next = PartitionFreelistEntry::Transform(nullptr);
  } else {
    page->freelist_head = nullptr;
  }
  return return_object;
}

bool PartitionBucket::SetNewActivePage() {
  PartitionPage* page = this->active_pages_head;
  if (page == PartitionPage::get_sentinel_page())
    return false;

  PartitionPage* next_page;

  for (; page; page = next_page) {
    next_page = page->next_page;
    DCHECK(page->bucket == this);
    DCHECK(page != this->empty_pages_head);
    DCHECK(page != this->decommitted_pages_head);

    if (LIKELY(page->is_active())) {
      // This page is usable because it has freelist entries, or has
      // unprovisioned slots we can create freelist entries from.
      this->active_pages_head = page;
      return true;
    }

    // Deal with empty and decommitted pages.
    if (LIKELY(page->is_empty())) {
      page->next_page = this->empty_pages_head;
      this->empty_pages_head = page;
    } else if (LIKELY(page->is_decommitted())) {
      page->next_page = this->decommitted_pages_head;
      this->decommitted_pages_head = page;
    } else {
      DCHECK(page->is_full());
      // If we get here, we found a full page. Skip over it too, and also
      // tag it as full (via a negative value). We need it tagged so that
      // free'ing can tell, and move it back into the active page list.
      page->num_allocated_slots = -page->num_allocated_slots;
      ++this->num_full_pages;
      // num_full_pages is a uint16_t for efficient packing so guard against
      // overflow to be safe.
      if (UNLIKELY(!this->num_full_pages))
        OnFull();
      // Not necessary but might help stop accidents.
      page->next_page = nullptr;
    }
  }

  this->active_pages_head = PartitionPage::get_sentinel_page();
  return false;
}

ALWAYS_INLINE PartitionDirectMapExtent* PartitionDirectMapExtent::FromPage(
    PartitionPage* page) {
  DCHECK(page->bucket->is_direct_mapped());
  return reinterpret_cast<PartitionDirectMapExtent*>(
      reinterpret_cast<char*>(page) + 3 * kPageMetadataSize);
}

ALWAYS_INLINE void PartitionPage::set_raw_size(size_t size) {
  size_t* raw_size_ptr = get_raw_size_ptr();
  if (UNLIKELY(raw_size_ptr != nullptr))
    *raw_size_ptr = size;
}

static ALWAYS_INLINE PartitionPage* PartitionDirectMap(PartitionRootBase* root,
                                                       int flags,
                                                       size_t raw_size) {
  size_t size = PartitionDirectMapSize(raw_size);

  // Because we need to fake looking like a super page, we need to allocate
  // a bunch of system pages more than "size":
  // - The first few system pages are the partition page in which the super
  // page metadata is stored. We fault just one system page out of a partition
  // page sized clump.
  // - We add a trailing guard page on 32-bit (on 64-bit we rely on the
  // massive address space plus randomization instead).
  size_t map_size = size + kPartitionPageSize;
#if !defined(ARCH_CPU_64_BITS)
  map_size += kSystemPageSize;
#endif
  // Round up to the allocation granularity.
  map_size += kPageAllocationGranularityOffsetMask;
  map_size &= kPageAllocationGranularityBaseMask;

  // TODO: these pages will be zero-filled. Consider internalizing an
  // allocZeroed() API so we can avoid a memset() entirely in this case.
  char* ptr = reinterpret_cast<char*>(
      AllocPages(nullptr, map_size, kSuperPageSize, PageReadWrite));
  if (UNLIKELY(!ptr))
    return nullptr;

  size_t committed_page_size = size + kSystemPageSize;
  root->total_size_of_direct_mapped_pages += committed_page_size;
  PartitionIncreaseCommittedPages(root, committed_page_size);

  char* slot = ptr + kPartitionPageSize;
  CHECK(SetSystemPagesAccess(ptr + (kSystemPageSize * 2),
                             kPartitionPageSize - (kSystemPageSize * 2),
                             PageInaccessible));
#if !defined(ARCH_CPU_64_BITS)
  CHECK(SetSystemPagesAccess(ptr, kSystemPageSize, PageInaccessible));
  CHECK(SetSystemPagesAccess(slot + size, kSystemPageSize, PageInaccessible));
#endif

  PartitionSuperPageExtentEntry* extent =
      reinterpret_cast<PartitionSuperPageExtentEntry*>(
          PartitionSuperPageToMetadataArea(ptr));
  extent->root = root;
  // The new structures are all located inside a fresh system page so they
  // will all be zeroed out. These DCHECKs are for documentation.
  DCHECK(!extent->super_page_base);
  DCHECK(!extent->super_pages_end);
  DCHECK(!extent->next);
  PartitionPage* page = PartitionPage::FromPointerNoAlignmentCheck(slot);
  PartitionBucket* bucket = reinterpret_cast<PartitionBucket*>(
      reinterpret_cast<char*>(page) + (kPageMetadataSize * 2));
  DCHECK(!page->next_page);
  DCHECK(!page->num_allocated_slots);
  DCHECK(!page->num_unprovisioned_slots);
  DCHECK(!page->page_offset);
  DCHECK(!page->empty_cache_index);
  page->bucket = bucket;
  page->freelist_head = reinterpret_cast<PartitionFreelistEntry*>(slot);
  PartitionFreelistEntry* next_entry =
      reinterpret_cast<PartitionFreelistEntry*>(slot);
  next_entry->next = PartitionFreelistEntry::Transform(nullptr);

  DCHECK(!bucket->active_pages_head);
  DCHECK(!bucket->empty_pages_head);
  DCHECK(!bucket->decommitted_pages_head);
  DCHECK(!bucket->num_system_pages_per_slot_span);
  DCHECK(!bucket->num_full_pages);
  bucket->slot_size = size;

  PartitionDirectMapExtent* map_extent =
      PartitionDirectMapExtent::FromPage(page);
  map_extent->map_size = map_size - kPartitionPageSize - kSystemPageSize;
  map_extent->bucket = bucket;

  // Maintain the doubly-linked list of all direct mappings.
  map_extent->next_extent = root->direct_map_list;
  if (map_extent->next_extent)
    map_extent->next_extent->prev_extent = map_extent;
  map_extent->prev_extent = nullptr;
  root->direct_map_list = map_extent;

  return page;
}

static ALWAYS_INLINE void PartitionDirectUnmap(PartitionPage* page) {
  PartitionRootBase* root = PartitionRootBase::FromPage(page);
  const PartitionDirectMapExtent* extent =
      PartitionDirectMapExtent::FromPage(page);
  size_t unmap_size = extent->map_size;

  // Maintain the doubly-linked list of all direct mappings.
  if (extent->prev_extent) {
    DCHECK(extent->prev_extent->next_extent == extent);
    extent->prev_extent->next_extent = extent->next_extent;
  } else {
    root->direct_map_list = extent->next_extent;
  }
  if (extent->next_extent) {
    DCHECK(extent->next_extent->prev_extent == extent);
    extent->next_extent->prev_extent = extent->prev_extent;
  }

  // Add on the size of the trailing guard page and preceeding partition
  // page.
  unmap_size += kPartitionPageSize + kSystemPageSize;

  size_t uncommitted_page_size = page->bucket->slot_size + kSystemPageSize;
  PartitionDecreaseCommittedPages(root, uncommitted_page_size);
  DCHECK(root->total_size_of_direct_mapped_pages >= uncommitted_page_size);
  root->total_size_of_direct_mapped_pages -= uncommitted_page_size;

  DCHECK(!(unmap_size & kPageAllocationGranularityOffsetMask));

  char* ptr = reinterpret_cast<char*>(PartitionPage::ToPointer(page));
  // Account for the mapping starting a partition page before the actual
  // allocation address.
  ptr -= kPartitionPageSize;

  FreePages(ptr, unmap_size);
}

void* PartitionBucket::SlowPathAlloc(PartitionRootBase* root,
                                     int flags,
                                     size_t size) {
  // The slow path is called when the freelist is empty.
  DCHECK(!this->active_pages_head->freelist_head);

  PartitionPage* new_page = nullptr;

  // For the PartitionRootGeneric::Alloc() API, we have a bunch of buckets
  // marked as special cases. We bounce them through to the slow path so that
  // we can still have a blazing fast hot path due to lack of corner-case
  // branches.
  //
  // Note: The ordering of the conditionals matter! In particular,
  // SetNewActivePage() has a side-effect even when returning
  // false where it sweeps the active page list and may move things into
  // the empty or decommitted lists which affects the subsequent conditional.
  bool returnNull = flags & PartitionAllocReturnNull;
  if (UNLIKELY(this->is_direct_mapped())) {
    DCHECK(size > kGenericMaxBucketed);
    DCHECK(this == get_sentinel_bucket());
    DCHECK(this->active_pages_head == &g_sentinel_page);
    if (size > kGenericMaxDirectMapped) {
      if (returnNull)
        return nullptr;
      PartitionExcessiveAllocationSize();
    }
    new_page = PartitionDirectMap(root, flags, size);
  } else if (LIKELY(this->SetNewActivePage())) {
    // First, did we find an active page in the active pages list?
    new_page = this->active_pages_head;
    DCHECK(new_page->is_active());
  } else if (LIKELY(this->empty_pages_head != nullptr) ||
             LIKELY(this->decommitted_pages_head != nullptr)) {
    // Second, look in our lists of empty and decommitted pages.
    // Check empty pages first, which are preferred, but beware that an
    // empty page might have been decommitted.
    while (LIKELY((new_page = this->empty_pages_head) != nullptr)) {
      DCHECK(new_page->bucket == this);
      DCHECK(new_page->is_empty() || new_page->is_decommitted());
      this->empty_pages_head = new_page->next_page;
      // Accept the empty page unless it got decommitted.
      if (new_page->freelist_head) {
        new_page->next_page = nullptr;
        break;
      }
      DCHECK(new_page->is_decommitted());
      new_page->next_page = this->decommitted_pages_head;
      this->decommitted_pages_head = new_page;
    }
    if (UNLIKELY(!new_page) &&
        LIKELY(this->decommitted_pages_head != nullptr)) {
      new_page = this->decommitted_pages_head;
      DCHECK(new_page->bucket == this);
      DCHECK(new_page->is_decommitted());
      this->decommitted_pages_head = new_page->next_page;
      void* addr = PartitionPage::ToPointer(new_page);
      PartitionRecommitSystemPages(root, addr,
                                   new_page->bucket->get_bytes_per_span());
      new_page->Reset();
    }
    DCHECK(new_page);
  } else {
    // Third. If we get here, we need a brand new page.
    uint16_t num_partition_pages = this->get_pages_per_slot_span();
    void* rawPages = AllocNewSlotSpan(root, flags, num_partition_pages);
    if (LIKELY(rawPages != nullptr)) {
      new_page = PartitionPage::FromPointerNoAlignmentCheck(rawPages);
      InitializeSlotSpan(new_page);
    }
  }

  // Bail if we had a memory allocation failure.
  if (UNLIKELY(!new_page)) {
    DCHECK(this->active_pages_head == &g_sentinel_page);
    if (returnNull)
      return nullptr;
    PartitionOutOfMemory(root);
  }

  // TODO(ajwong): Is there a way to avoid the reading of bucket here?
  // It seems like in many of the conditional branches above, |this| ==
  // |new_page->bucket|. Maybe pull this into another function?
  PartitionBucket* bucket = new_page->bucket;
  DCHECK(bucket != get_sentinel_bucket());
  bucket->active_pages_head = new_page;
  new_page->set_raw_size(size);

  // If we found an active page with free slots, or an empty page, we have a
  // usable freelist head.
  if (LIKELY(new_page->freelist_head != nullptr)) {
    PartitionFreelistEntry* entry = new_page->freelist_head;
    PartitionFreelistEntry* new_head =
        PartitionFreelistEntry::Transform(entry->next);
    new_page->freelist_head = new_head;
    new_page->num_allocated_slots++;
    return entry;
  }
  // Otherwise, we need to build the freelist.
  DCHECK(new_page->num_unprovisioned_slots);
  return AllocAndFillFreelist(new_page);
}

PartitionBucket* PartitionBucket::get_sentinel_bucket() {
  return &g_sentinel_bucket;
}

static ALWAYS_INLINE void PartitionDecommitPage(PartitionRootBase* root,
                                                PartitionPage* page) {
  DCHECK(page->is_empty());
  DCHECK(!page->bucket->is_direct_mapped());
  void* addr = PartitionPage::ToPointer(page);
  PartitionDecommitSystemPages(root, addr, page->bucket->get_bytes_per_span());

  // We actually leave the decommitted page in the active list. We'll sweep
  // it on to the decommitted page list when we next walk the active page
  // list.
  // Pulling this trick enables us to use a singly-linked page list for all
  // cases, which is critical in keeping the page metadata structure down to
  // 32 bytes in size.
  page->freelist_head = nullptr;
  page->num_unprovisioned_slots = 0;
  DCHECK(page->is_decommitted());
}

static void PartitionDecommitPageIfPossible(PartitionRootBase* root,
                                            PartitionPage* page) {
  DCHECK(page->empty_cache_index >= 0);
  DCHECK(static_cast<unsigned>(page->empty_cache_index) < kMaxFreeableSpans);
  DCHECK(page == root->global_empty_page_ring[page->empty_cache_index]);
  page->empty_cache_index = -1;
  if (page->is_empty())
    PartitionDecommitPage(root, page);
}

static ALWAYS_INLINE void PartitionRegisterEmptyPage(PartitionPage* page) {
  DCHECK(page->is_empty());
  PartitionRootBase* root = PartitionRootBase::FromPage(page);

  // If the page is already registered as empty, give it another life.
  if (page->empty_cache_index != -1) {
    DCHECK(page->empty_cache_index >= 0);
    DCHECK(static_cast<unsigned>(page->empty_cache_index) < kMaxFreeableSpans);
    DCHECK(root->global_empty_page_ring[page->empty_cache_index] == page);
    root->global_empty_page_ring[page->empty_cache_index] = nullptr;
  }

  int16_t current_index = root->global_empty_page_ring_index;
  PartitionPage* pageToDecommit = root->global_empty_page_ring[current_index];
  // The page might well have been re-activated, filled up, etc. before we get
  // around to looking at it here.
  if (pageToDecommit)
    PartitionDecommitPageIfPossible(root, pageToDecommit);

  // We put the empty slot span on our global list of "pages that were once
  // empty". thus providing it a bit of breathing room to get re-used before
  // we really free it. This improves performance, particularly on Mac OS X
  // which has subpar memory management performance.
  root->global_empty_page_ring[current_index] = page;
  page->empty_cache_index = current_index;
  ++current_index;
  if (current_index == kMaxFreeableSpans)
    current_index = 0;
  root->global_empty_page_ring_index = current_index;
}

static void PartitionDecommitEmptyPages(PartitionRootBase* root) {
  for (size_t i = 0; i < kMaxFreeableSpans; ++i) {
    PartitionPage* page = root->global_empty_page_ring[i];
    if (page)
      PartitionDecommitPageIfPossible(root, page);
    root->global_empty_page_ring[i] = nullptr;
  }
}

PartitionPage* PartitionPage::get_sentinel_page() {
  return &g_sentinel_page;
}

void PartitionPage::FreeSlowPath() {
  DCHECK(this != &g_sentinel_page);
  if (LIKELY(this->num_allocated_slots == 0)) {
    // Page became fully unused.
    if (UNLIKELY(bucket->is_direct_mapped())) {
      PartitionDirectUnmap(this);
      return;
    }
    // If it's the current active page, change it. We bounce the page to
    // the empty list as a force towards defragmentation.
    if (LIKELY(this == bucket->active_pages_head))
      bucket->SetNewActivePage();
    DCHECK(bucket->active_pages_head != this);

    set_raw_size(0);
    DCHECK(!get_raw_size());

    PartitionRegisterEmptyPage(this);
  } else {
    DCHECK(!bucket->is_direct_mapped());
    // Ensure that the page is full. That's the only valid case if we
    // arrive here.
    DCHECK(this->num_allocated_slots < 0);
    // A transition of num_allocated_slots from 0 to -1 is not legal, and
    // likely indicates a double-free.
    CHECK(this->num_allocated_slots != -1);
    this->num_allocated_slots = -this->num_allocated_slots - 2;
    DCHECK(this->num_allocated_slots == bucket->get_slots_per_span() - 1);
    // Fully used page became partially used. It must be put back on the
    // non-full page list. Also make it the current page to increase the
    // chances of it being filled up again. The old current page will be
    // the next page.
    DCHECK(!this->next_page);
    if (LIKELY(bucket->active_pages_head != &g_sentinel_page))
      this->next_page = bucket->active_pages_head;
    bucket->active_pages_head = this;
    --bucket->num_full_pages;
    // Special case: for a partition page with just a single slot, it may
    // now be empty and we want to run it through the empty logic.
    if (UNLIKELY(this->num_allocated_slots == 0))
      FreeSlowPath();
  }
}

bool PartitionReallocDirectMappedInPlace(PartitionRootGeneric* root,
                                         PartitionPage* page,
                                         size_t raw_size) {
  DCHECK(page->bucket->is_direct_mapped());

  raw_size = PartitionCookieSizeAdjustAdd(raw_size);

  // Note that the new size might be a bucketed size; this function is called
  // whenever we're reallocating a direct mapped allocation.
  size_t new_size = PartitionDirectMapSize(raw_size);
  if (new_size < kGenericMinDirectMappedDownsize)
    return false;

  // bucket->slot_size is the current size of the allocation.
  size_t current_size = page->bucket->slot_size;
  if (new_size == current_size)
    return true;

  char* char_ptr = static_cast<char*>(PartitionPage::ToPointer(page));

  if (new_size < current_size) {
    size_t map_size = PartitionDirectMapExtent::FromPage(page)->map_size;

    // Don't reallocate in-place if new size is less than 80 % of the full
    // map size, to avoid holding on to too much unused address space.
    if ((new_size / kSystemPageSize) * 5 < (map_size / kSystemPageSize) * 4)
      return false;

    // Shrink by decommitting unneeded pages and making them inaccessible.
    size_t decommitSize = current_size - new_size;
    PartitionDecommitSystemPages(root, char_ptr + new_size, decommitSize);
    CHECK(SetSystemPagesAccess(char_ptr + new_size, decommitSize,
                               PageInaccessible));
  } else if (new_size <= PartitionDirectMapExtent::FromPage(page)->map_size) {
    // Grow within the actually allocated memory. Just need to make the
    // pages accessible again.
    size_t recommit_size = new_size - current_size;
    CHECK(SetSystemPagesAccess(char_ptr + current_size, recommit_size,
                               PageReadWrite));
    PartitionRecommitSystemPages(root, char_ptr + current_size, recommit_size);

#if DCHECK_IS_ON()
    memset(char_ptr + current_size, kUninitializedByte, recommit_size);
#endif
  } else {
    // We can't perform the realloc in-place.
    // TODO: support this too when possible.
    return false;
  }

#if DCHECK_IS_ON()
  // Write a new trailing cookie.
  PartitionCookieWriteValue(char_ptr + raw_size - kCookieSize);
#endif

  page->set_raw_size(raw_size);
  DCHECK(page->get_raw_size() == raw_size);

  page->bucket->slot_size = new_size;
  return true;
}

void* PartitionRootGeneric::Realloc(void* ptr,
                                    size_t new_size,
                                    const char* type_name) {
#if defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
  return realloc(ptr, new_size);
#else
  if (UNLIKELY(!ptr))
    return this->Alloc(new_size, type_name);
  if (UNLIKELY(!new_size)) {
    this->Free(ptr);
    return nullptr;
  }

  if (new_size > kGenericMaxDirectMapped)
    PartitionExcessiveAllocationSize();

  PartitionPage* page =
      PartitionPage::FromPointer(PartitionCookieFreePointerAdjust(ptr));
  // TODO(palmer): See if we can afford to make this a CHECK.
  DCHECK(PartitionPage::IsPointerValid(page));

  if (UNLIKELY(page->bucket->is_direct_mapped())) {
    // We may be able to perform the realloc in place by changing the
    // accessibility of memory pages and, if reducing the size, decommitting
    // them.
    if (PartitionReallocDirectMappedInPlace(this, page, new_size)) {
      PartitionAllocHooks::ReallocHookIfEnabled(ptr, ptr, new_size, type_name);
      return ptr;
    }
  }

  size_t actual_new_size = this->ActualSize(new_size);
  size_t actual_old_size = PartitionAllocGetSize(ptr);

  // TODO: note that tcmalloc will "ignore" a downsizing realloc() unless the
  // new size is a significant percentage smaller. We could do the same if we
  // determine it is a win.
  if (actual_new_size == actual_old_size) {
    // Trying to allocate a block of size new_size would give us a block of
    // the same size as the one we've already got, so re-use the allocation
    // after updating statistics (and cookies, if present).
    page->set_raw_size(PartitionCookieSizeAdjustAdd(new_size));
#if DCHECK_IS_ON()
    // Write a new trailing cookie when it is possible to keep track of
    // |new_size| via the raw size pointer.
    if (page->get_raw_size_ptr())
      PartitionCookieWriteValue(static_cast<char*>(ptr) + new_size);
#endif
    return ptr;
  }

  // This realloc cannot be resized in-place. Sadness.
  void* ret = this->Alloc(new_size, type_name);
  size_t copy_size = actual_old_size;
  if (new_size < copy_size)
    copy_size = new_size;

  memcpy(ret, ptr, copy_size);
  this->Free(ptr);
  return ret;
#endif
}

static size_t PartitionPurgePage(PartitionPage* page, bool discard) {
  const PartitionBucket* bucket = page->bucket;
  size_t slot_size = bucket->slot_size;
  if (slot_size < kSystemPageSize || !page->num_allocated_slots)
    return 0;

  size_t bucket_num_slots = bucket->get_slots_per_span();
  size_t discardable_bytes = 0;

  size_t raw_size = page->get_raw_size();
  if (raw_size) {
    uint32_t usedBytes = static_cast<uint32_t>(RoundUpToSystemPage(raw_size));
    discardable_bytes = bucket->slot_size - usedBytes;
    if (discardable_bytes && discard) {
      char* ptr = reinterpret_cast<char*>(PartitionPage::ToPointer(page));
      ptr += usedBytes;
      DiscardSystemPages(ptr, discardable_bytes);
    }
    return discardable_bytes;
  }

  constexpr size_t kMaxSlotCount =
      (kPartitionPageSize * kMaxPartitionPagesPerSlotSpan) / kSystemPageSize;
  DCHECK(bucket_num_slots <= kMaxSlotCount);
  DCHECK(page->num_unprovisioned_slots < bucket_num_slots);
  size_t num_slots = bucket_num_slots - page->num_unprovisioned_slots;
  char slot_usage[kMaxSlotCount];
#if !defined(OS_WIN)
  // The last freelist entry should not be discarded when using OS_WIN.
  // DiscardVirtualMemory makes the contents of discarded memory undefined.
  size_t last_slot = static_cast<size_t>(-1);
#endif
  memset(slot_usage, 1, num_slots);
  char* ptr = reinterpret_cast<char*>(PartitionPage::ToPointer(page));
  // First, walk the freelist for this page and make a bitmap of which slots
  // are not in use.
  for (PartitionFreelistEntry* entry = page->freelist_head; entry; /**/) {
    size_t slotIndex = (reinterpret_cast<char*>(entry) - ptr) / slot_size;
    DCHECK(slotIndex < num_slots);
    slot_usage[slotIndex] = 0;
    entry = PartitionFreelistEntry::Transform(entry->next);
#if !defined(OS_WIN)
    // If we have a slot where the masked freelist entry is 0, we can
    // actually discard that freelist entry because touching a discarded
    // page is guaranteed to return original content or 0.
    // (Note that this optimization won't fire on big endian machines
    // because the masking function is negation.)
    if (!PartitionFreelistEntry::Transform(entry))
      last_slot = slotIndex;
#endif
  }

  // If the slot(s) at the end of the slot span are not in used, we can
  // truncate them entirely and rewrite the freelist.
  size_t truncated_slots = 0;
  while (!slot_usage[num_slots - 1]) {
    truncated_slots++;
    num_slots--;
    DCHECK(num_slots);
  }
  // First, do the work of calculating the discardable bytes. Don't actually
  // discard anything unless the discard flag was passed in.
  if (truncated_slots) {
    size_t unprovisioned_bytes = 0;
    char* begin_ptr = ptr + (num_slots * slot_size);
    char* end_ptr = begin_ptr + (slot_size * truncated_slots);
    begin_ptr = reinterpret_cast<char*>(
        RoundUpToSystemPage(reinterpret_cast<size_t>(begin_ptr)));
    // We round the end pointer here up and not down because we're at the
    // end of a slot span, so we "own" all the way up the page boundary.
    end_ptr = reinterpret_cast<char*>(
        RoundUpToSystemPage(reinterpret_cast<size_t>(end_ptr)));
    DCHECK(end_ptr <= ptr + bucket->get_bytes_per_span());
    if (begin_ptr < end_ptr) {
      unprovisioned_bytes = end_ptr - begin_ptr;
      discardable_bytes += unprovisioned_bytes;
    }
    if (unprovisioned_bytes && discard) {
      DCHECK(truncated_slots > 0);
      size_t num_new_entries = 0;
      page->num_unprovisioned_slots += static_cast<uint16_t>(truncated_slots);
      // Rewrite the freelist.
      PartitionFreelistEntry** entry_ptr = &page->freelist_head;
      for (size_t slotIndex = 0; slotIndex < num_slots; ++slotIndex) {
        if (slot_usage[slotIndex])
          continue;
        auto* entry = reinterpret_cast<PartitionFreelistEntry*>(
            ptr + (slot_size * slotIndex));
        *entry_ptr = PartitionFreelistEntry::Transform(entry);
        entry_ptr = reinterpret_cast<PartitionFreelistEntry**>(entry);
        num_new_entries++;
#if !defined(OS_WIN)
        last_slot = slotIndex;
#endif
      }
      // Terminate the freelist chain.
      *entry_ptr = nullptr;
      // The freelist head is stored unmasked.
      page->freelist_head =
          PartitionFreelistEntry::Transform(page->freelist_head);
      DCHECK(num_new_entries == num_slots - page->num_allocated_slots);
      // Discard the memory.
      DiscardSystemPages(begin_ptr, unprovisioned_bytes);
    }
  }

  // Next, walk the slots and for any not in use, consider where the system
  // page boundaries occur. We can release any system pages back to the
  // system as long as we don't interfere with a freelist pointer or an
  // adjacent slot.
  for (size_t i = 0; i < num_slots; ++i) {
    if (slot_usage[i])
      continue;
    // The first address we can safely discard is just after the freelist
    // pointer. There's one quirk: if the freelist pointer is actually a
    // null, we can discard that pointer value too.
    char* begin_ptr = ptr + (i * slot_size);
    char* end_ptr = begin_ptr + slot_size;
#if !defined(OS_WIN)
    if (i != last_slot)
      begin_ptr += sizeof(PartitionFreelistEntry);
#else
    begin_ptr += sizeof(PartitionFreelistEntry);
#endif
    begin_ptr = reinterpret_cast<char*>(
        RoundUpToSystemPage(reinterpret_cast<size_t>(begin_ptr)));
    end_ptr = reinterpret_cast<char*>(
        RoundDownToSystemPage(reinterpret_cast<size_t>(end_ptr)));
    if (begin_ptr < end_ptr) {
      size_t partial_slot_bytes = end_ptr - begin_ptr;
      discardable_bytes += partial_slot_bytes;
      if (discard)
        DiscardSystemPages(begin_ptr, partial_slot_bytes);
    }
  }
  return discardable_bytes;
}

static void PartitionPurgeBucket(PartitionBucket* bucket) {
  if (bucket->active_pages_head != &g_sentinel_page) {
    for (PartitionPage* page = bucket->active_pages_head; page;
         page = page->next_page) {
      DCHECK(page != &g_sentinel_page);
      PartitionPurgePage(page, true);
    }
  }
}

void PartitionRoot::PurgeMemory(int flags) {
  if (flags & PartitionPurgeDecommitEmptyPages)
    PartitionDecommitEmptyPages(this);
  // We don't currently do anything for PartitionPurgeDiscardUnusedSystemPages
  // here because that flag is only useful for allocations >= system page
  // size. We only have allocations that large inside generic partitions
  // at the moment.
}

void PartitionRootGeneric::PurgeMemory(int flags) {
  subtle::SpinLock::Guard guard(this->lock);
  if (flags & PartitionPurgeDecommitEmptyPages)
    PartitionDecommitEmptyPages(this);
  if (flags & PartitionPurgeDiscardUnusedSystemPages) {
    for (size_t i = 0; i < kGenericNumBuckets; ++i) {
      PartitionBucket* bucket = &this->buckets[i];
      if (bucket->slot_size >= kSystemPageSize)
        PartitionPurgeBucket(bucket);
    }
  }
}

static void PartitionDumpPageStats(PartitionBucketMemoryStats* stats_out,
                                   PartitionPage* page) {
  uint16_t bucket_num_slots = page->bucket->get_slots_per_span();

  if (page->is_decommitted()) {
    ++stats_out->num_decommitted_pages;
    return;
  }

  stats_out->discardable_bytes += PartitionPurgePage(page, false);

  size_t raw_size = page->get_raw_size();
  if (raw_size) {
    stats_out->active_bytes += static_cast<uint32_t>(raw_size);
  } else {
    stats_out->active_bytes +=
        (page->num_allocated_slots * stats_out->bucket_slot_size);
  }

  size_t page_bytes_resident =
      RoundUpToSystemPage((bucket_num_slots - page->num_unprovisioned_slots) *
                          stats_out->bucket_slot_size);
  stats_out->resident_bytes += page_bytes_resident;
  if (page->is_empty()) {
    stats_out->decommittable_bytes += page_bytes_resident;
    ++stats_out->num_empty_pages;
  } else if (page->is_full()) {
    ++stats_out->num_full_pages;
  } else {
    DCHECK(page->is_active());
    ++stats_out->num_active_pages;
  }
}

static void PartitionDumpBucketStats(PartitionBucketMemoryStats* stats_out,
                                     const PartitionBucket* bucket) {
  DCHECK(!bucket->is_direct_mapped());
  stats_out->is_valid = false;
  // If the active page list is empty (== &g_sentinel_page),
  // the bucket might still need to be reported if it has a list of empty,
  // decommitted or full pages.
  if (bucket->active_pages_head == &g_sentinel_page &&
      !bucket->empty_pages_head && !bucket->decommitted_pages_head &&
      !bucket->num_full_pages)
    return;

  memset(stats_out, '\0', sizeof(*stats_out));
  stats_out->is_valid = true;
  stats_out->is_direct_map = false;
  stats_out->num_full_pages = static_cast<size_t>(bucket->num_full_pages);
  stats_out->bucket_slot_size = bucket->slot_size;
  uint16_t bucket_num_slots = bucket->get_slots_per_span();
  size_t bucket_useful_storage = stats_out->bucket_slot_size * bucket_num_slots;
  stats_out->allocated_page_size = bucket->get_bytes_per_span();
  stats_out->active_bytes = bucket->num_full_pages * bucket_useful_storage;
  stats_out->resident_bytes =
      bucket->num_full_pages * stats_out->allocated_page_size;

  for (PartitionPage* page = bucket->empty_pages_head; page;
       page = page->next_page) {
    DCHECK(page->is_empty() || page->is_decommitted());
    PartitionDumpPageStats(stats_out, page);
  }
  for (PartitionPage* page = bucket->decommitted_pages_head; page;
       page = page->next_page) {
    DCHECK(page->is_decommitted());
    PartitionDumpPageStats(stats_out, page);
  }

  if (bucket->active_pages_head != &g_sentinel_page) {
    for (PartitionPage* page = bucket->active_pages_head; page;
         page = page->next_page) {
      DCHECK(page != &g_sentinel_page);
      PartitionDumpPageStats(stats_out, page);
    }
  }
}

void PartitionRootGeneric::DumpStats(const char* partition_name,
                                     bool is_light_dump,
                                     PartitionStatsDumper* dumper) {
  PartitionMemoryStats stats = {0};
  stats.total_mmapped_bytes =
      this->total_size_of_super_pages + this->total_size_of_direct_mapped_pages;
  stats.total_committed_bytes = this->total_size_of_committed_pages;

  size_t direct_mapped_allocations_total_size = 0;

  static const size_t kMaxReportableDirectMaps = 4096;

  // Allocate on the heap rather than on the stack to avoid stack overflow
  // skirmishes (on Windows, in particular).
  std::unique_ptr<uint32_t[]> direct_map_lengths = nullptr;
  if (!is_light_dump) {
    direct_map_lengths =
        std::unique_ptr<uint32_t[]>(new uint32_t[kMaxReportableDirectMaps]);
  }

  PartitionBucketMemoryStats bucket_stats[kGenericNumBuckets];
  size_t num_direct_mapped_allocations = 0;
  {
    subtle::SpinLock::Guard guard(this->lock);

    for (size_t i = 0; i < kGenericNumBuckets; ++i) {
      const PartitionBucket* bucket = &this->buckets[i];
      // Don't report the pseudo buckets that the generic allocator sets up in
      // order to preserve a fast size->bucket map (see
      // PartitionRootGeneric::Init() for details).
      if (!bucket->active_pages_head)
        bucket_stats[i].is_valid = false;
      else
        PartitionDumpBucketStats(&bucket_stats[i], bucket);
      if (bucket_stats[i].is_valid) {
        stats.total_resident_bytes += bucket_stats[i].resident_bytes;
        stats.total_active_bytes += bucket_stats[i].active_bytes;
        stats.total_decommittable_bytes += bucket_stats[i].decommittable_bytes;
        stats.total_discardable_bytes += bucket_stats[i].discardable_bytes;
      }
    }

    for (PartitionDirectMapExtent *extent = this->direct_map_list;
         extent && num_direct_mapped_allocations < kMaxReportableDirectMaps;
         extent = extent->next_extent, ++num_direct_mapped_allocations) {
      DCHECK(!extent->next_extent ||
             extent->next_extent->prev_extent == extent);
      size_t slot_size = extent->bucket->slot_size;
      direct_mapped_allocations_total_size += slot_size;
      if (is_light_dump)
        continue;
      direct_map_lengths[num_direct_mapped_allocations] = slot_size;
    }
  }

  if (!is_light_dump) {
    // Call |PartitionsDumpBucketStats| after collecting stats because it can
    // try to allocate using |PartitionRootGeneric::Alloc()| and it can't
    // obtain the lock.
    for (size_t i = 0; i < kGenericNumBuckets; ++i) {
      if (bucket_stats[i].is_valid)
        dumper->PartitionsDumpBucketStats(partition_name, &bucket_stats[i]);
    }

    for (size_t i = 0; i < num_direct_mapped_allocations; ++i) {
      uint32_t size = direct_map_lengths[i];

      PartitionBucketMemoryStats mapped_stats = {};
      mapped_stats.is_valid = true;
      mapped_stats.is_direct_map = true;
      mapped_stats.num_full_pages = 1;
      mapped_stats.allocated_page_size = size;
      mapped_stats.bucket_slot_size = size;
      mapped_stats.active_bytes = size;
      mapped_stats.resident_bytes = size;
      dumper->PartitionsDumpBucketStats(partition_name, &mapped_stats);
    }
  }

  stats.total_resident_bytes += direct_mapped_allocations_total_size;
  stats.total_active_bytes += direct_mapped_allocations_total_size;
  dumper->PartitionDumpTotals(partition_name, &stats);
}

void PartitionRoot::DumpStats(const char* partition_name,
                              bool is_light_dump,
                              PartitionStatsDumper* dumper) {
  PartitionMemoryStats stats = {0};
  stats.total_mmapped_bytes = this->total_size_of_super_pages;
  stats.total_committed_bytes = this->total_size_of_committed_pages;
  DCHECK(!this->total_size_of_direct_mapped_pages);

  static const size_t kMaxReportableBuckets = 4096 / sizeof(void*);
  std::unique_ptr<PartitionBucketMemoryStats[]> memory_stats;
  if (!is_light_dump)
    memory_stats = std::unique_ptr<PartitionBucketMemoryStats[]>(
        new PartitionBucketMemoryStats[kMaxReportableBuckets]);

  const size_t partitionNumBuckets = this->num_buckets;
  DCHECK(partitionNumBuckets <= kMaxReportableBuckets);

  for (size_t i = 0; i < partitionNumBuckets; ++i) {
    PartitionBucketMemoryStats bucket_stats = {0};
    PartitionDumpBucketStats(&bucket_stats, &this->buckets()[i]);
    if (bucket_stats.is_valid) {
      stats.total_resident_bytes += bucket_stats.resident_bytes;
      stats.total_active_bytes += bucket_stats.active_bytes;
      stats.total_decommittable_bytes += bucket_stats.decommittable_bytes;
      stats.total_discardable_bytes += bucket_stats.discardable_bytes;
    }
    if (!is_light_dump) {
      if (bucket_stats.is_valid)
        memory_stats[i] = bucket_stats;
      else
        memory_stats[i].is_valid = false;
    }
  }
  if (!is_light_dump) {
    // PartitionsDumpBucketStats is called after collecting stats because it
    // can use PartitionRoot::Alloc() to allocate and this can affect the
    // statistics.
    for (size_t i = 0; i < partitionNumBuckets; ++i) {
      if (memory_stats[i].is_valid)
        dumper->PartitionsDumpBucketStats(partition_name, &memory_stats[i]);
    }
  }
  dumper->PartitionDumpTotals(partition_name, &stats);
}

}  // namespace base
