// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/fake_command_buffer_helper.h"

namespace media {

FakeCommandBufferHelper::FakeCommandBufferHelper(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {}

FakeCommandBufferHelper::~FakeCommandBufferHelper() = default;

void FakeCommandBufferHelper::StubLost() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  has_stub_ = false;
  is_context_lost_ = true;
  is_context_current_ = false;
  service_ids_.clear();
  waits_.clear();
}

void FakeCommandBufferHelper::ContextLost() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  is_context_lost_ = true;
  is_context_current_ = false;
}

void FakeCommandBufferHelper::CurrentContextLost() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  is_context_current_ = false;
}

bool FakeCommandBufferHelper::HasTexture(GLuint service_id) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return service_ids_.count(service_id);
}

void FakeCommandBufferHelper::ReleaseSyncToken(gpu::SyncToken sync_token) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(waits_.count(sync_token));
  task_runner_->PostTask(FROM_HERE, std::move(waits_[sync_token]));
  waits_.erase(sync_token);
}

bool FakeCommandBufferHelper::MakeContextCurrent() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  is_context_current_ = !is_context_lost_;
  return is_context_current_;
}

GLuint FakeCommandBufferHelper::CreateTexture(GLenum target,
                                              GLenum internal_format,
                                              GLsizei width,
                                              GLsizei height,
                                              GLenum format,
                                              GLenum type) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(is_context_current_);
  GLuint service_id = next_service_id_++;
  service_ids_.insert(service_id);
  return service_id;
}

void FakeCommandBufferHelper::DestroyTexture(GLuint service_id) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(is_context_current_);
  DCHECK(service_ids_.count(service_id));
  service_ids_.erase(service_id);
}

gpu::Mailbox FakeCommandBufferHelper::CreateMailbox(GLuint service_id) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(service_ids_.count(service_id));
  return gpu::Mailbox::Generate();
}

void FakeCommandBufferHelper::SetCleared(GLuint service_id) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(service_ids_.count(service_id));
}

void FakeCommandBufferHelper::WaitForSyncToken(gpu::SyncToken sync_token,
                                               base::OnceClosure done_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!waits_.count(sync_token));
  waits_.emplace(sync_token, std::move(done_cb));
}

}  // namespace media
