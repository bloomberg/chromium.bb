// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/texture_layer.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "cc/layers/texture_layer_client.h"
#include "cc/layers/texture_layer_impl.h"
#include "cc/trees/layer_tree_host.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"

namespace cc {

scoped_refptr<TextureLayer> TextureLayer::Create(TextureLayerClient* client) {
  return scoped_refptr<TextureLayer>(new TextureLayer(client, false));
}

scoped_refptr<TextureLayer> TextureLayer::CreateForMailbox(
    TextureLayerClient* client) {
  return scoped_refptr<TextureLayer>(new TextureLayer(client, true));
}

TextureLayer::TextureLayer(TextureLayerClient* client, bool uses_mailbox)
    : Layer(),
      client_(client),
      uses_mailbox_(uses_mailbox),
      flipped_(true),
      uv_top_left_(0.f, 0.f),
      uv_bottom_right_(1.f, 1.f),
      premultiplied_alpha_(true),
      rate_limit_context_(false),
      context_lost_(false),
      content_committed_(false),
      texture_id_(0),
      needs_set_mailbox_(false) {
  vertex_opacity_[0] = 1.0f;
  vertex_opacity_[1] = 1.0f;
  vertex_opacity_[2] = 1.0f;
  vertex_opacity_[3] = 1.0f;
}

TextureLayer::~TextureLayer() {
  if (layer_tree_host()) {
    if (texture_id_)
      layer_tree_host()->AcquireLayerTextures();
    if (rate_limit_context_ && client_)
      layer_tree_host()->StopRateLimiter(client_->Context3d());
  }
}

void TextureLayer::ClearClient() {
  client_ = NULL;
  if (uses_mailbox_)
    SetTextureMailbox(TextureMailbox());
  else
    SetTextureId(0);
}

scoped_ptr<LayerImpl> TextureLayer::CreateLayerImpl(LayerTreeImpl* tree_impl) {
  return TextureLayerImpl::Create(tree_impl, id(), uses_mailbox_).
      PassAs<LayerImpl>();
}

void TextureLayer::SetFlipped(bool flipped) {
  if (flipped_ == flipped)
    return;
  flipped_ = flipped;
  SetNeedsCommit();
}

void TextureLayer::SetUV(gfx::PointF top_left, gfx::PointF bottom_right) {
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

void TextureLayer::SetRateLimitContext(bool rate_limit) {
  if (!rate_limit && rate_limit_context_ && client_ && layer_tree_host())
    layer_tree_host()->StopRateLimiter(client_->Context3d());

  rate_limit_context_ = rate_limit;
}

void TextureLayer::SetTextureId(unsigned id) {
  DCHECK(!uses_mailbox_);
  if (texture_id_ == id)
    return;
  if (texture_id_ && layer_tree_host())
    layer_tree_host()->AcquireLayerTextures();
  texture_id_ = id;
  SetNeedsCommit();
}

void TextureLayer::SetTextureMailbox(const TextureMailbox& mailbox) {
  DCHECK(uses_mailbox_);
  DCHECK(!mailbox.IsValid() || !holder_ref_ ||
         !mailbox.Equals(holder_ref_->holder()->mailbox()));
  // If we never commited the mailbox, we need to release it here.
  if (mailbox.IsValid())
    holder_ref_ = MailboxHolder::Create(mailbox);
  else
    holder_ref_.reset();
  needs_set_mailbox_ = true;
  SetNeedsCommit();
}

void TextureLayer::WillModifyTexture() {
  if (layer_tree_host() && (DrawsContent() || content_committed_)) {
    layer_tree_host()->AcquireLayerTextures();
    content_committed_ = false;
  }
}

void TextureLayer::SetNeedsDisplayRect(const gfx::RectF& dirty_rect) {
  Layer::SetNeedsDisplayRect(dirty_rect);

  if (rate_limit_context_ && client_ && layer_tree_host() && DrawsContent())
    layer_tree_host()->StartRateLimiter(client_->Context3d());
}

void TextureLayer::SetLayerTreeHost(LayerTreeHost* host) {
  if (texture_id_ && layer_tree_host() && host != layer_tree_host())
    layer_tree_host()->AcquireLayerTextures();
  // If we're removed from the tree, the TextureLayerImpl will be destroyed, and
  // we will need to set the mailbox again on a new TextureLayerImpl the next
  // time we push.
  if (!host && uses_mailbox_ && holder_ref_)
    needs_set_mailbox_ = true;
  Layer::SetLayerTreeHost(host);
}

bool TextureLayer::DrawsContent() const {
  return (client_ || texture_id_ || holder_ref_) &&
         !context_lost_ && Layer::DrawsContent();
}

void TextureLayer::Update(ResourceUpdateQueue* queue,
                          const OcclusionTracker* occlusion,
                          RenderingStats* stats) {
  if (client_) {
    if (uses_mailbox_) {
      TextureMailbox mailbox;
      if (client_->PrepareTextureMailbox(&mailbox)) {
        if (mailbox.IsTexture())
          DCHECK(client_->Context3d());
        SetTextureMailbox(mailbox);
      }
    } else {
      DCHECK(client_->Context3d());
      texture_id_ = client_->PrepareTexture(queue);
    }
    context_lost_ = client_->Context3d() &&
        client_->Context3d()->getGraphicsResetStatusARB() != GL_NO_ERROR;
  }

  needs_display_ = false;
}

void TextureLayer::PushPropertiesTo(LayerImpl* layer) {
  Layer::PushPropertiesTo(layer);

  TextureLayerImpl* texture_layer = static_cast<TextureLayerImpl*>(layer);
  texture_layer->set_flipped(flipped_);
  texture_layer->set_uv_top_left(uv_top_left_);
  texture_layer->set_uv_bottom_right(uv_bottom_right_);
  texture_layer->set_vertex_opacity(vertex_opacity_);
  texture_layer->set_premultiplied_alpha(premultiplied_alpha_);
  if (uses_mailbox_ && needs_set_mailbox_) {
    TextureMailbox texture_mailbox;
    if (holder_ref_) {
      MailboxHolder* holder = holder_ref_->holder();
      TextureMailbox::ReleaseCallback callback =
          holder->GetCallbackForImplThread();
      texture_mailbox = holder->mailbox().CopyWithNewCallback(callback);
    }
    texture_layer->SetTextureMailbox(texture_mailbox);
    needs_set_mailbox_ = false;
  } else {
    texture_layer->set_texture_id(texture_id_);
  }
  content_committed_ = DrawsContent();
}

bool TextureLayer::BlocksPendingCommit() const {
  // Double-buffered texture layers need to be blocked until they can be made
  // triple-buffered.  Single-buffered layers already prevent draws, so
  // can block too for simplicity.
  return DrawsContent();
}

bool TextureLayer::CanClipSelf() const {
  return true;
}

TextureLayer::MailboxHolder::MainThreadReference::MainThreadReference(
    MailboxHolder* holder)
    : holder_(holder) {
  holder_->InternalAddRef();
}

TextureLayer::MailboxHolder::MainThreadReference::~MainThreadReference() {
  holder_->InternalRelease();
}

TextureLayer::MailboxHolder::MailboxHolder(const TextureMailbox& mailbox)
    : message_loop_(base::MessageLoopProxy::current()),
      internal_references_(0),
      mailbox_(mailbox),
      sync_point_(mailbox.sync_point()),
      is_lost_(false) {
}

TextureLayer::MailboxHolder::~MailboxHolder() {
  DCHECK_EQ(0u, internal_references_);
}

scoped_ptr<TextureLayer::MailboxHolder::MainThreadReference>
TextureLayer::MailboxHolder::Create(const TextureMailbox& mailbox) {
  return scoped_ptr<MainThreadReference>(new MainThreadReference(
      new MailboxHolder(mailbox)));
}

void TextureLayer::MailboxHolder::Return(unsigned sync_point, bool is_lost) {
  sync_point_ = sync_point;
  is_lost_ = is_lost;
}

TextureMailbox::ReleaseCallback
TextureLayer::MailboxHolder::GetCallbackForImplThread() {
  // We can't call GetCallbackForImplThread if we released the main thread
  // reference.
  DCHECK_GT(internal_references_, 0u);
  InternalAddRef();
  return base::Bind(&MailboxHolder::ReturnAndReleaseOnImplThread, this);
}

void TextureLayer::MailboxHolder::InternalAddRef() {
  ++internal_references_;
}

void TextureLayer::MailboxHolder::InternalRelease() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  if (!--internal_references_) {
    mailbox_.RunReleaseCallback(sync_point_, is_lost_);
    mailbox_ = TextureMailbox();
  }
}

void TextureLayer::MailboxHolder::ReturnAndReleaseOnMainThread(
    unsigned sync_point, bool is_lost) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  Return(sync_point, is_lost);
  InternalRelease();
}

void TextureLayer::MailboxHolder::ReturnAndReleaseOnImplThread(
    unsigned sync_point, bool is_lost) {
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &MailboxHolder::ReturnAndReleaseOnMainThread,
      this, sync_point, is_lost));
}

}  // namespace cc
