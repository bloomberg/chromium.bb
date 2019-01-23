// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image_representation.h"

#include <inttypes.h>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "ui/gl/trace_util.h"

namespace gpu {
// Overrides for flat_set lookups:
bool operator<(const std::unique_ptr<SharedImageBacking>& lhs,
               const std::unique_ptr<SharedImageBacking>& rhs) {
  return lhs->mailbox() < rhs->mailbox();
}

bool operator<(const Mailbox& lhs,
               const std::unique_ptr<SharedImageBacking>& rhs) {
  return lhs < rhs->mailbox();
}

bool operator<(const std::unique_ptr<SharedImageBacking>& lhs,
               const Mailbox& rhs) {
  return lhs->mailbox() < rhs;
}

SharedImageManager::SharedImageManager() = default;

SharedImageManager::~SharedImageManager() {
  DCHECK(images_.empty());
}

std::unique_ptr<SharedImageRepresentationFactoryRef>
SharedImageManager::Register(std::unique_ptr<SharedImageBacking> backing,
                             MemoryTypeTracker* tracker) {
  DCHECK(backing->mailbox().IsSharedImage());
  if (images_.find(backing->mailbox()) != images_.end()) {
    LOG(ERROR) << "ShraedImageManager::Register: Trying to register an "
                  "already registered mailbox.";
    backing->Destroy();
    return nullptr;
  }
  auto factory_ref = std::make_unique<SharedImageRepresentationFactoryRef>(
      this, backing.get(), tracker);
  images_.emplace(std::move(backing));
  return factory_ref;
}

void SharedImageManager::OnContextLost(const Mailbox& mailbox) {
  auto found = images_.find(mailbox);
  if (found == images_.end()) {
    LOG(ERROR) << "SharedImageManager::OnContextLost: Trying to mark constext "
                  "lost on a non existent mailbox.";
    return;
  }

  (*found)->OnContextLost();
}

std::unique_ptr<SharedImageRepresentationGLTexture>
SharedImageManager::ProduceGLTexture(const Mailbox& mailbox,
                                     MemoryTypeTracker* tracker) {
  auto found = images_.find(mailbox);
  if (found == images_.end()) {
    LOG(ERROR) << "SharedImageManager::ProduceGLTexture: Trying to produce a "
                  "representation from a non-existent mailbox.";
    return nullptr;
  }

  auto representation = (*found)->ProduceGLTexture(this, tracker);
  if (!representation) {
    LOG(ERROR) << "SharedImageManager::ProduceGLTexture: Trying to produce a "
                  "representation from an incompatible mailbox.";
    return nullptr;
  }

  return representation;
}

std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
SharedImageManager::ProduceGLTexturePassthrough(const Mailbox& mailbox,
                                                MemoryTypeTracker* tracker) {
  auto found = images_.find(mailbox);
  if (found == images_.end()) {
    LOG(ERROR) << "SharedImageManager::ProduceGLTexturePassthrough: Trying to "
                  "produce a representation from a non-existent mailbox.";
    return nullptr;
  }

  auto representation = (*found)->ProduceGLTexturePassthrough(this, tracker);
  if (!representation) {
    LOG(ERROR) << "SharedImageManager::ProduceGLTexturePassthrough: Trying to "
                  "produce a representation from an incompatible mailbox.";
    return nullptr;
  }

  return representation;
}

std::unique_ptr<SharedImageRepresentationSkia> SharedImageManager::ProduceSkia(
    const Mailbox& mailbox,
    MemoryTypeTracker* tracker) {
  auto found = images_.find(mailbox);
  if (found == images_.end()) {
    LOG(ERROR) << "SharedImageManager::ProduceSkia: Trying to Produce a "
                  "Skia representation from a non-existent mailbox.";
    return nullptr;
  }

  auto representation = (*found)->ProduceSkia(this, tracker);
  if (!representation) {
    LOG(ERROR) << "SharedImageManager::ProduceSkia: Trying to produce a "
                  "Skia representation from an incompatible mailbox.";
    return nullptr;
  }

  return representation;
}

void SharedImageManager::OnRepresentationDestroyed(
    const Mailbox& mailbox,
    SharedImageRepresentation* representation) {
  auto found = images_.find(mailbox);
  if (found == images_.end()) {
    LOG(ERROR) << "SharedImageManager::OnRepresentationDestroyed: Trying to "
                  "destroy a non existent mailbox.";
    return;
  }

  // TODO(piman): When the original (factory) representation is destroyed, we
  // should treat the backing as pending destruction and prevent additional
  // representations from being created. This will help avoid races due to a
  // consumer getting lucky with timing due to a representation inadvertently
  // extending a backing's lifetime.
  (*found)->ReleaseRef(representation);
  if (!(*found)->HasAnyRefs())
    images_.erase(found);
}

void SharedImageManager::OnMemoryDump(const Mailbox& mailbox,
                                      base::trace_event::ProcessMemoryDump* pmd,
                                      int client_id,
                                      uint64_t client_tracing_id) {
  auto found = images_.find(mailbox);
  if (found == images_.end()) {
    LOG(ERROR) << "SharedImageManager::OnMemoryDump: Trying to dump memory for "
                  "a non existent mailbox.";
    return;
  }

  auto* backing = found->get();
  size_t estimated_size = backing->estimated_size();
  if (estimated_size == 0)
    return;

  // Unique name in the process.
  std::string dump_name =
      base::StringPrintf("gpu/shared-images/client_0x%" PRIX32 "/mailbox_%s",
                         client_id, mailbox.ToDebugString().c_str());

  base::trace_event::MemoryAllocatorDump* dump =
      pmd->CreateAllocatorDump(dump_name);
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  estimated_size);
  // Add a mailbox guid which expresses shared ownership with the client
  // process.
  // This must match the client-side.
  auto client_guid = GetSharedImageGUIDForTracing(mailbox);
  pmd->CreateSharedGlobalAllocatorDump(client_guid);
  pmd->AddOwnershipEdge(dump->guid(), client_guid);

  // Allow the SharedImageBacking to attach additional data to the dump
  // or dump additional sub-paths.
  backing->OnMemoryDump(dump_name, dump, pmd, client_tracing_id);
}

}  // namespace gpu
