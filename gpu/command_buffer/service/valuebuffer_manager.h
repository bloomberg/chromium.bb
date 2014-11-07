// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_VALUEBUFFER_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_VALUEBUFFER_MANAGER_H_

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/gpu_export.h"

namespace gpu {
namespace gles2 {

class ValuebufferManager;

union ValueState {
  float float_value[4];
  int int_value[4];
};

class GPU_EXPORT Valuebuffer : public base::RefCounted<Valuebuffer> {
 public:
  Valuebuffer(ValuebufferManager* manager, GLuint client_id);

  GLuint client_id() const { return client_id_; }

  bool IsDeleted() const { return client_id_ == 0; }

  void MarkAsValid() { has_been_bound_ = true; }

  bool IsValid() const { return has_been_bound_ && !IsDeleted(); }

  void AddSubscription(GLenum subscription);
  void RemoveSubscription(GLenum subscription);

  // Returns true if this Valuebuffer is subscribed to subscription
  bool IsSubscribed(GLenum subscription);

  // Returns the active state for a given target in this Valuebuffer
  // returns NULL if target state doesn't exist
  const ValueState* GetState(GLenum target) const;

 private:
  friend class ValuebufferManager;
  friend class base::RefCounted<Valuebuffer>;

  typedef base::hash_map<GLenum, ValueState> StateMap;
  typedef base::hash_set<GLenum> SubscriptionSet;

  ~Valuebuffer();

  void UpdateState(const StateMap& pending_state);

  void MarkAsDeleted() { client_id_ = 0; }

  // ValuebufferManager that owns this Valuebuffer.
  ValuebufferManager* manager_;

  // Client side Valuebuffer id.
  GLuint client_id_;

  // Whether this Valuebuffer has ever been bound.
  bool has_been_bound_;

  SubscriptionSet subscriptions_;

  StateMap active_state_map_;
};

class GPU_EXPORT ValuebufferManager {
 public:
  ValuebufferManager();
  ~ValuebufferManager();

  // Must call before destruction.
  void Destroy();

  // Creates a Valuebuffer for the given Valuebuffer ids.
  void CreateValuebuffer(GLuint client_id);

  // Gets the Valuebuffer for the given Valuebuffer id.
  Valuebuffer* GetValuebuffer(GLuint client_id);

  // Removes a Valuebuffer for the given Valuebuffer id.
  void RemoveValuebuffer(GLuint client_id);

  // Updates the value state for the given Valuebuffer
  void UpdateValuebufferState(Valuebuffer* valuebuffer);

  // Gets the state for the given subscription target
  void UpdateValueState(GLenum target, const ValueState& state);

  static uint32 ApiTypeForSubscriptionTarget(GLenum target);

 private:
  friend class Valuebuffer;

  typedef base::hash_map<GLuint, scoped_refptr<Valuebuffer>> ValuebufferMap;

  void StartTracking(Valuebuffer* valuebuffer);
  void StopTracking(Valuebuffer* valuebuffer);

  // Counts the number of Valuebuffer allocated with 'this' as its manager.
  // Allows to check no Valuebuffer will outlive this.
  unsigned valuebuffer_count_;

  // Info for each Valuebuffer in the system.
  ValuebufferMap valuebuffer_map_;

  // Current value state in the system
  Valuebuffer::StateMap pending_state_map_;

  DISALLOW_COPY_AND_ASSIGN(ValuebufferManager);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_VALUEBUFFER_MANAGER_H_
