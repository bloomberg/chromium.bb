// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/html_viewer/discardable_memory_allocator.h"

#include "base/memory/discardable_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"

namespace html_viewer {

// Represents an actual memory chunk. This is an object owned by
// DiscardableMemoryAllocator. DiscardableMemoryChunkImpl are passed to the
// rest of the program, and access this memory through a weak ptr.
class DiscardableMemoryAllocator::OwnedMemoryChunk {
 public:
  OwnedMemoryChunk(size_t size, DiscardableMemoryAllocator* allocator)
      : is_locked_(true),
        size_(size),
        memory_(new uint8_t[size]),
        allocator_(allocator),
        weak_factory_(this) {}
  ~OwnedMemoryChunk() {}

  void Lock() {
    DCHECK(!is_locked_);
    is_locked_ = true;
    allocator_->NotifyLocked(unlocked_position_);
  }

  void Unlock() {
    DCHECK(is_locked_);
    is_locked_ = false;
    unlocked_position_ = allocator_->NotifyUnlocked(this);
  }

  bool is_locked() const { return is_locked_; }
  size_t size() const { return size_; }
  void* Memory() const {
    DCHECK(is_locked_);
    return memory_.get();
  }

  base::WeakPtr<OwnedMemoryChunk> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  bool is_locked_;
  size_t size_;
  scoped_ptr<uint8_t[]> memory_;
  DiscardableMemoryAllocator* allocator_;

  std::list<OwnedMemoryChunk*>::iterator unlocked_position_;

  base::WeakPtrFactory<OwnedMemoryChunk> weak_factory_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(OwnedMemoryChunk);
};

namespace {

// Interface to the rest of the program. These objects are owned outside of the
// allocator and wrap a weak ptr.
class DiscardableMemoryChunkImpl : public base::DiscardableMemory {
 public:
  explicit DiscardableMemoryChunkImpl(
      base::WeakPtr<DiscardableMemoryAllocator::OwnedMemoryChunk> chunk)
      : memory_chunk_(chunk) {}
  ~DiscardableMemoryChunkImpl() override {
    // Either the memory chunk is invalid (because the backing has gone away),
    // or the memory chunk is unlocked (because leaving the chunk locked once
    // we deallocate means the chunk will never get cleaned up).
    DCHECK(!memory_chunk_ || !memory_chunk_->is_locked());
  }

  // Overridden from DiscardableMemoryChunk:
  bool Lock() override {
    if (!memory_chunk_)
      return false;

    memory_chunk_->Lock();
    return true;
  }

  void Unlock() override {
    DCHECK(memory_chunk_);
    memory_chunk_->Unlock();
  }

  void* Memory() const override {
    if (memory_chunk_)
      return memory_chunk_->Memory();
    return nullptr;
  }

 private:
  base::WeakPtr<DiscardableMemoryAllocator::OwnedMemoryChunk> memory_chunk_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(DiscardableMemoryChunkImpl);
};

}  // namespace

DiscardableMemoryAllocator::DiscardableMemoryAllocator(
    size_t desired_max_memory)
    : desired_max_memory_(desired_max_memory),
      total_live_memory_(0u),
      locked_chunks_(0) {
}

DiscardableMemoryAllocator::~DiscardableMemoryAllocator() {
  DCHECK_EQ(0, locked_chunks_);
  STLDeleteElements(&live_unlocked_chunks_);
}

scoped_ptr<base::DiscardableMemory>
DiscardableMemoryAllocator::AllocateLockedDiscardableMemory(size_t size) {
  OwnedMemoryChunk* chunk = new OwnedMemoryChunk(size, this);
  total_live_memory_ += size;
  locked_chunks_++;

  // Go through the list of unlocked live chunks starting from the least
  // recently used, freeing as many as we can until we get our size under the
  // desired maximum.
  auto it = live_unlocked_chunks_.begin();
  while (total_live_memory_ > desired_max_memory_ &&
         it != live_unlocked_chunks_.end()) {
    total_live_memory_ -= (*it)->size();
    delete *it;
    it = live_unlocked_chunks_.erase(it);
  }

  return make_scoped_ptr(new DiscardableMemoryChunkImpl(chunk->GetWeakPtr()));
}

std::list<DiscardableMemoryAllocator::OwnedMemoryChunk*>::iterator
DiscardableMemoryAllocator::NotifyUnlocked(
    DiscardableMemoryAllocator::OwnedMemoryChunk* chunk) {
  locked_chunks_--;
  return live_unlocked_chunks_.insert(live_unlocked_chunks_.end(), chunk);
}

void DiscardableMemoryAllocator::NotifyLocked(
    std::list<OwnedMemoryChunk*>::iterator it) {
  locked_chunks_++;
  live_unlocked_chunks_.erase(it);
}

}  // namespace html_viewer
