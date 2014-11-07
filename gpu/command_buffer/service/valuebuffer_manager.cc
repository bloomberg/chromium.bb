// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/valuebuffer_manager.h"

#include "gpu/command_buffer/service/program_manager.h"

namespace gpu {
namespace gles2 {

Valuebuffer::Valuebuffer(ValuebufferManager* manager, GLuint client_id)
    : manager_(manager), client_id_(client_id), has_been_bound_(false) {
  manager_->StartTracking(this);
}

Valuebuffer::~Valuebuffer() {
  if (manager_) {
    manager_->StopTracking(this);
    manager_ = NULL;
  }
}

void Valuebuffer::AddSubscription(GLenum subscription) {
  subscriptions_.insert(subscription);
}

void Valuebuffer::RemoveSubscription(GLenum subscription) {
  subscriptions_.erase(subscription);
}

bool Valuebuffer::IsSubscribed(GLenum subscription) {
  return subscriptions_.find(subscription) != subscriptions_.end();
}

const ValueState *Valuebuffer::GetState(GLenum target) const {
  StateMap::const_iterator it = active_state_map_.find(target);
  return it != active_state_map_.end() ? &it->second : NULL;
}

void Valuebuffer::UpdateState(const StateMap& pending_state) {
  for (SubscriptionSet::const_iterator it = subscriptions_.begin();
       it != subscriptions_.end(); ++it) {
    StateMap::const_iterator pending_state_it = pending_state.find((*it));
    if (pending_state_it != pending_state.end()) {
      active_state_map_[pending_state_it->first] = pending_state_it->second;
    }
  }
}

ValuebufferManager::ValuebufferManager()
    : valuebuffer_count_(0) {
}

ValuebufferManager::~ValuebufferManager() {
  DCHECK(valuebuffer_map_.empty());
  DCHECK(pending_state_map_.empty());
  // If this triggers, that means something is keeping a reference to
  // a Valuebuffer belonging to this.
  CHECK_EQ(valuebuffer_count_, 0u);
}

void ValuebufferManager::Destroy() {
  valuebuffer_map_.clear();
  pending_state_map_.clear();
}

void ValuebufferManager::StartTracking(Valuebuffer* /* valuebuffer */) {
  ++valuebuffer_count_;
}

void ValuebufferManager::StopTracking(Valuebuffer* /* valuebuffer */) {
  --valuebuffer_count_;
}

void ValuebufferManager::CreateValuebuffer(GLuint client_id) {
  scoped_refptr<Valuebuffer> valuebuffer(new Valuebuffer(this, client_id));
  std::pair<ValuebufferMap::iterator, bool> result =
      valuebuffer_map_.insert(std::make_pair(client_id, valuebuffer));
  DCHECK(result.second);
}

Valuebuffer* ValuebufferManager::GetValuebuffer(GLuint client_id) {
  ValuebufferMap::iterator it = valuebuffer_map_.find(client_id);
  return it != valuebuffer_map_.end() ? it->second.get() : NULL;
}

void ValuebufferManager::RemoveValuebuffer(GLuint client_id) {
  ValuebufferMap::iterator it = valuebuffer_map_.find(client_id);
  if (it != valuebuffer_map_.end()) {
    Valuebuffer* valuebuffer = it->second.get();
    valuebuffer->MarkAsDeleted();
    valuebuffer_map_.erase(it);
  }
}

void ValuebufferManager::UpdateValuebufferState(Valuebuffer* valuebuffer) {
  DCHECK(valuebuffer);
  valuebuffer->UpdateState(pending_state_map_);
}

void ValuebufferManager::UpdateValueState(
    GLenum target, const ValueState& state) {
  pending_state_map_[target] = state;
}

uint32 ValuebufferManager::ApiTypeForSubscriptionTarget(GLenum target) {
  switch (target) {
    case GL_MOUSE_POSITION_CHROMIUM:
      return Program::kUniform2i;
  }
  NOTREACHED() << "Unhandled uniform subscription target " << target;
  return Program::kUniformNone;
}

}  // namespace gles2
}  // namespace gpu
