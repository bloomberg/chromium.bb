// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/texture_layer.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/simple_enclosed_region.h"
#include "cc/layers/texture_layer_client.h"
#include "cc/layers/texture_layer_impl.h"
#include "cc/trees/layer_tree_host.h"
#include "components/viz/common/resources/single_release_callback.h"

namespace cc {

scoped_refptr<TextureLayer> TextureLayer::CreateForMailbox(
    TextureLayerClient* client) {
  return scoped_refptr<TextureLayer>(new TextureLayer(client));
}

TextureLayer::TextureLayer(TextureLayerClient* client) : client_(client) {}

TextureLayer::~TextureLayer() = default;

void TextureLayer::ClearClient() {
  client_ = nullptr;
  ClearTexture();
  UpdateDrawsContent(HasDrawableContent());
}

void TextureLayer::ClearTexture() {
  SetTransferableResource(viz::TransferableResource(), nullptr);
}

std::unique_ptr<LayerImpl> TextureLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return TextureLayerImpl::Create(tree_impl, id());
}

void TextureLayer::SetFlipped(bool flipped) {
  if (flipped_ == flipped)
    return;
  flipped_ = flipped;
  SetNeedsCommit();
}

void TextureLayer::SetNearestNeighbor(bool nearest_neighbor) {
  if (nearest_neighbor_ == nearest_neighbor)
    return;
  nearest_neighbor_ = nearest_neighbor;
  SetNeedsCommit();
}

void TextureLayer::SetUV(const gfx::PointF& top_left,
                         const gfx::PointF& bottom_right) {
  if (uv_top_left_ == top_left && uv_bottom_right_ == bottom_right)
    return;
  uv_top_left_ = top_left;
  uv_bottom_right_ = bottom_right;
  SetNeedsCommit();
}

void TextureLayer::SetVertexOpacity(float bottom_left,
                                    float top_left,
                                    float top_right,
                                    float bottom_right) {
  // Indexing according to the quad vertex generation:
  // 1--2
  // |  |
  // 0--3
  if (vertex_opacity_[0] == bottom_left &&
      vertex_opacity_[1] == top_left &&
      vertex_opacity_[2] == top_right &&
      vertex_opacity_[3] == bottom_right)
    return;
  vertex_opacity_[0] = bottom_left;
  vertex_opacity_[1] = top_left;
  vertex_opacity_[2] = top_right;
  vertex_opacity_[3] = bottom_right;
  SetNeedsCommit();
}

void TextureLayer::SetPremultipliedAlpha(bool premultiplied_alpha) {
  if (premultiplied_alpha_ == premultiplied_alpha)
    return;
  premultiplied_alpha_ = premultiplied_alpha;
  SetNeedsCommit();
}

void TextureLayer::SetBlendBackgroundColor(bool blend) {
  if (blend_background_color_ == blend)
    return;
  blend_background_color_ = blend;
  SetNeedsCommit();
}

void TextureLayer::SetTransferableResourceInternal(
    const viz::TransferableResource& resource,
    std::unique_ptr<viz::SingleReleaseCallback> release_callback,
    bool requires_commit) {
  DCHECK(resource.mailbox_holder.mailbox.IsZero() || !holder_ref_ ||
         resource != holder_ref_->holder()->resource());
  DCHECK_EQ(resource.mailbox_holder.mailbox.IsZero(), !release_callback);

  // If we never commited the mailbox, we need to release it here.
  if (!resource.mailbox_holder.mailbox.IsZero()) {
    holder_ref_ = TransferableResourceHolder::Create(
        resource, std::move(release_callback));
  } else {
    holder_ref_ = nullptr;
  }
  needs_set_resource_ = true;
  // If we are within a commit, no need to do it again immediately after.
  if (requires_commit)
    SetNeedsCommit();
  else
    SetNeedsPushProperties();

  UpdateDrawsContent(HasDrawableContent());
  // The active frame needs to be replaced and the mailbox returned before the
  // commit is called complete.
  SetNextCommitWaitsForActivation();
}

void TextureLayer::SetTransferableResource(
    const viz::TransferableResource& resource,
    std::unique_ptr<viz::SingleReleaseCallback> release_callback) {
  bool requires_commit = true;
  SetTransferableResourceInternal(resource, std::move(release_callback),
                                  requires_commit);
}

void TextureLayer::SetNeedsDisplayRect(const gfx::Rect& dirty_rect) {
  Layer::SetNeedsDisplayRect(dirty_rect);
}

void TextureLayer::SetLayerTreeHost(LayerTreeHost* host) {
  if (layer_tree_host() == host) {
    Layer::SetLayerTreeHost(host);
    return;
  }

  // If we're removed from the tree, the TextureLayerImpl will be destroyed, and
  // we will need to set the mailbox again on a new TextureLayerImpl the next
  // time we push.
  if (!host && holder_ref_) {
    needs_set_resource_ = true;
    // The active frame needs to be replaced and the mailbox returned before the
    // commit is called complete.
    SetNextCommitWaitsForActivation();
  }
  Layer::SetLayerTreeHost(host);
}

bool TextureLayer::HasDrawableContent() const {
  return (client_ || holder_ref_) && Layer::HasDrawableContent();
}

bool TextureLayer::Update() {
  bool updated = Layer::Update();
  if (client_) {
    viz::TransferableResource resource;
    std::unique_ptr<viz::SingleReleaseCallback> release_callback;
    if (client_->PrepareTransferableResource(&resource, &release_callback)) {
      // Already within a commit, no need to do another one immediately.
      bool requires_commit = false;
      SetTransferableResourceInternal(resource, std::move(release_callback),
                                      requires_commit);
      updated = true;
    }
  }

  // SetTransferableResource could be called externally and the same mailbox
  // used for different textures.  Such callers notify this layer that the
  // texture has changed by calling SetNeedsDisplay, so check for that here.
  return updated || !update_rect().IsEmpty();
}

bool TextureLayer::IsSnapped() {
  return true;
}

void TextureLayer::PushPropertiesTo(LayerImpl* layer) {
  Layer::PushPropertiesTo(layer);
  TRACE_EVENT0("cc", "TextureLayer::PushPropertiesTo");

  TextureLayerImpl* texture_layer = static_cast<TextureLayerImpl*>(layer);
  texture_layer->SetFlipped(flipped_);
  texture_layer->SetNearestNeighbor(nearest_neighbor_);
  texture_layer->SetUVTopLeft(uv_top_left_);
  texture_layer->SetUVBottomRight(uv_bottom_right_);
  texture_layer->SetVertexOpacity(vertex_opacity_);
  texture_layer->SetPremultipliedAlpha(premultiplied_alpha_);
  texture_layer->SetBlendBackgroundColor(blend_background_color_);
  if (needs_set_resource_) {
    viz::TransferableResource resource;
    std::unique_ptr<viz::SingleReleaseCallback> release_callback;
    if (holder_ref_) {
      TransferableResourceHolder* holder = holder_ref_->holder();
      resource = holder->resource();
      release_callback = holder->GetCallbackForImplThread(
          layer_tree_host()->GetTaskRunnerProvider()->MainThreadTaskRunner());
    }
    texture_layer->SetTransferableResource(resource,
                                           std::move(release_callback));
    needs_set_resource_ = false;
  }
}

TextureLayer::TransferableResourceHolder::MainThreadReference::
    MainThreadReference(TransferableResourceHolder* holder)
    : holder_(holder) {
  holder_->InternalAddRef();
}

TextureLayer::TransferableResourceHolder::MainThreadReference::
    ~MainThreadReference() {
  holder_->InternalRelease();
}

TextureLayer::TransferableResourceHolder::TransferableResourceHolder(
    const viz::TransferableResource& resource,
    std::unique_ptr<viz::SingleReleaseCallback> release_callback)
    : internal_references_(0),
      resource_(resource),
      release_callback_(std::move(release_callback)),
      sync_token_(resource.mailbox_holder.sync_token),
      is_lost_(false) {}

TextureLayer::TransferableResourceHolder::~TransferableResourceHolder() {
  DCHECK_EQ(0u, internal_references_);
}

std::unique_ptr<TextureLayer::TransferableResourceHolder::MainThreadReference>
TextureLayer::TransferableResourceHolder::Create(
    const viz::TransferableResource& resource,
    std::unique_ptr<viz::SingleReleaseCallback> release_callback) {
  return std::make_unique<MainThreadReference>(
      new TransferableResourceHolder(resource, std::move(release_callback)));
}

void TextureLayer::TransferableResourceHolder::Return(
    const gpu::SyncToken& sync_token,
    bool is_lost) {
  base::AutoLock lock(arguments_lock_);
  sync_token_ = sync_token;
  is_lost_ = is_lost;
}

std::unique_ptr<viz::SingleReleaseCallback>
TextureLayer::TransferableResourceHolder::GetCallbackForImplThread(
    scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner) {
  // We can't call GetCallbackForImplThread if we released the main thread
  // reference.
  DCHECK_GT(internal_references_, 0u);
  InternalAddRef();
  return viz::SingleReleaseCallback::Create(
      base::Bind(&TransferableResourceHolder::ReturnAndReleaseOnImplThread,
                 this, std::move(main_thread_task_runner)));
}

void TextureLayer::TransferableResourceHolder::InternalAddRef() {
  ++internal_references_;
}

void TextureLayer::TransferableResourceHolder::InternalRelease() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
  if (!--internal_references_) {
    release_callback_->Run(sync_token_, is_lost_);
    resource_ = viz::TransferableResource();
    release_callback_ = nullptr;
  }
}

void TextureLayer::TransferableResourceHolder::ReturnAndReleaseOnImplThread(
    const scoped_refptr<base::SequencedTaskRunner>& main_thread_task_runner,
    const gpu::SyncToken& sync_token,
    bool is_lost) {
  Return(sync_token, is_lost);
  main_thread_task_runner->PostTask(
      FROM_HERE,
      base::Bind(&TransferableResourceHolder::InternalRelease, this));
}

}  // namespace cc
