// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/texture_layer.h"

#include "cc/base/thread.h"
#include "cc/layers/texture_layer_client.h"
#include "cc/layers/texture_layer_impl.h"
#include "cc/trees/layer_tree_host.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"

namespace cc {

static void RunCallbackOnMainThread(
    const TextureMailbox::ReleaseCallback& callback,
    unsigned sync_point) {
  callback.Run(sync_point);
}

static void PostCallbackToMainThread(
    Thread* main_thread,
    const TextureMailbox::ReleaseCallback& callback,
    unsigned sync_point) {
  main_thread->PostTask(
      base::Bind(&RunCallbackOnMainThread, callback, sync_point));
}

scoped_refptr<TextureLayer> TextureLayer::Create(TextureLayerClient* client) {
  return scoped_refptr<TextureLayer>(new TextureLayer(client, false));
}

scoped_refptr<TextureLayer> TextureLayer::CreateForMailbox() {
  return scoped_refptr<TextureLayer>(new TextureLayer(NULL, true));
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
      texture_id_(0),
      content_committed_(false),
      own_mailbox_(false) {
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
  if (own_mailbox_)
    texture_mailbox_.RunReleaseCallback(texture_mailbox_.sync_point());
}

void TextureLayer::ClearClient() {
  client_ = NULL;
  SetTextureId(0);
}

scoped_ptr<LayerImpl> TextureLayer::CreateLayerImpl(LayerTreeImpl* tree_impl) {
  return TextureLayerImpl::Create(tree_impl, id(), uses_mailbox_).
      PassAs<LayerImpl>();
}

void TextureLayer::SetFlipped(bool flipped) {
  flipped_ = flipped;
  SetNeedsCommit();
}

void TextureLayer::SetUV(gfx::PointF top_left, gfx::PointF bottom_right) {
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
  vertex_opacity_[0] = bottom_left;
  vertex_opacity_[1] = top_left;
  vertex_opacity_[2] = top_right;
  vertex_opacity_[3] = bottom_right;
  SetNeedsCommit();
}

void TextureLayer::SetPremultipliedAlpha(bool premultiplied_alpha) {
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
  DCHECK(mailbox.IsEmpty() || !mailbox.Equals(texture_mailbox_));
  // If we never commited the mailbox, we need to release it here
  if (own_mailbox_)
    texture_mailbox_.RunReleaseCallback(texture_mailbox_.sync_point());
  texture_mailbox_ = mailbox;
  own_mailbox_ = true;

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
  Layer::SetLayerTreeHost(host);
}

bool TextureLayer::DrawsContent() const {
  return (client_ || texture_id_ || !texture_mailbox_.IsEmpty()) &&
         !context_lost_ && Layer::DrawsContent();
}

void TextureLayer::Update(ResourceUpdateQueue* queue,
                          const OcclusionTracker* occlusion,
                          RenderingStats* stats) {
  if (client_) {
    texture_id_ = client_->PrepareTexture(queue);
    context_lost_ =
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
  if (uses_mailbox_ && own_mailbox_) {
    Thread* main_thread = layer_tree_host()->proxy()->MainThread();
    TextureMailbox::ReleaseCallback callback;
    if (!texture_mailbox_.IsEmpty())
      callback = base::Bind(
          &PostCallbackToMainThread, main_thread, texture_mailbox_.callback());
    texture_layer->SetTextureMailbox(TextureMailbox(
        texture_mailbox_.name(), callback, texture_mailbox_.sync_point()));
    own_mailbox_ = false;
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

}  // namespace cc
