// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_TEXTURE_LAYER_H_
#define CC_LAYERS_TEXTURE_LAYER_H_

#include <string>

#include "base/callback.h"
#include "cc/base/cc_export.h"
#include "cc/layers/layer.h"
#include "cc/resources/texture_mailbox.h"

namespace WebKit { class WebGraphicsContext3D; }

namespace base {
class MessageLoopProxy;
}

namespace cc {

class TextureLayerClient;

// A Layer containing a the rendered output of a plugin instance.
class CC_EXPORT TextureLayer : public Layer {
 public:
  // If this texture layer requires special preparation logic for each frame
  // driven by the compositor, pass in a non-nil client. Pass in a nil client
  // pointer if texture updates are driven by an external process.
  static scoped_refptr<TextureLayer> Create(TextureLayerClient* client);

  // Used when mailbox names are specified instead of texture IDs.
  static scoped_refptr<TextureLayer> CreateForMailbox(
      TextureLayerClient* client);

  void ClearClient();

  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;

  // Sets whether this texture should be Y-flipped at draw time. Defaults to
  // true.
  void SetFlipped(bool flipped);

  // Sets a UV transform to be used at draw time. Defaults to (0, 0) and (1, 1).
  void SetUV(gfx::PointF top_left, gfx::PointF bottom_right);

  // Sets an opacity value per vertex. It will be multiplied by the layer
  // opacity value.
  void SetVertexOpacity(float bottom_left,
                        float top_left,
                        float top_right,
                        float bottom_right);

  // Sets whether the alpha channel is premultiplied or unpremultiplied.
  // Defaults to true.
  void SetPremultipliedAlpha(bool premultiplied_alpha);

  // Sets whether the texture should be blended with the background color
  // at draw time. Defaults to false.
  void SetBlendBackgroundColor(bool blend);

  // Sets whether this context should rate limit on damage to prevent too many
  // frames from being queued up before the compositor gets a chance to run.
  // Requires a non-nil client.  Defaults to false.
  void SetRateLimitContext(bool rate_limit);

  // Code path for plugins which supply their own texture ID.
  void SetTextureId(unsigned texture_id);

  // Code path for plugins which supply their own mailbox.
  bool uses_mailbox() const { return uses_mailbox_; }
  void SetTextureMailbox(const TextureMailbox& mailbox);

  void WillModifyTexture();

  virtual void SetNeedsDisplayRect(const gfx::RectF& dirty_rect) OVERRIDE;

  virtual void SetLayerTreeHost(LayerTreeHost* layer_tree_host) OVERRIDE;
  virtual bool DrawsContent() const OVERRIDE;
  virtual bool Update(ResourceUpdateQueue* queue,
                      const OcclusionTracker* occlusion) OVERRIDE;
  virtual void PushPropertiesTo(LayerImpl* layer) OVERRIDE;
  virtual Region VisibleContentOpaqueRegion() const OVERRIDE;
  virtual bool BlocksPendingCommit() const OVERRIDE;

  virtual bool CanClipSelf() const OVERRIDE;

 protected:
  TextureLayer(TextureLayerClient* client, bool uses_mailbox);
  virtual ~TextureLayer();

 private:
  class MailboxHolder : public base::RefCountedThreadSafe<MailboxHolder> {
   public:
    class MainThreadReference {
     public:
      explicit MainThreadReference(MailboxHolder* holder);
      ~MainThreadReference();
      MailboxHolder* holder() { return holder_.get(); }

     private:
      scoped_refptr<MailboxHolder> holder_;
      DISALLOW_COPY_AND_ASSIGN(MainThreadReference);
    };

    static scoped_ptr<MainThreadReference> Create(
        const TextureMailbox& mailbox);

    const TextureMailbox& mailbox() const { return mailbox_; }
    void Return(unsigned sync_point, bool is_lost);

    // Gets a ReleaseCallback that can be called from another thread. Note: the
    // caller must ensure the callback is called.
    TextureMailbox::ReleaseCallback GetCallbackForImplThread();

   private:
    friend class base::RefCountedThreadSafe<MailboxHolder>;
    friend class MainThreadReference;
    explicit MailboxHolder(const TextureMailbox& mailbox);
    ~MailboxHolder();
    void InternalAddRef();
    void InternalRelease();
    void ReturnAndReleaseOnMainThread(unsigned sync_point, bool is_lost);
    void ReturnAndReleaseOnImplThread(unsigned sync_point, bool is_lost);

    // Thread safety notes: except for the thread-safe message_loop_, all fields
    // are only used on the main thread, or on the impl thread during commit
    // where the main thread is blocked.
    const scoped_refptr<base::MessageLoopProxy> message_loop_;
    unsigned internal_references_;
    TextureMailbox mailbox_;
    unsigned sync_point_;
    bool is_lost_;
    DISALLOW_COPY_AND_ASSIGN(MailboxHolder);
  };

  TextureLayerClient* client_;
  bool uses_mailbox_;

  bool flipped_;
  gfx::PointF uv_top_left_;
  gfx::PointF uv_bottom_right_;
  // [bottom left, top left, top right, bottom right]
  float vertex_opacity_[4];
  bool premultiplied_alpha_;
  bool blend_background_color_;
  bool rate_limit_context_;
  bool content_committed_;

  unsigned texture_id_;
  scoped_ptr<MailboxHolder::MainThreadReference> holder_ref_;
  bool needs_set_mailbox_;

  DISALLOW_COPY_AND_ASSIGN(TextureLayer);
};

}  // namespace cc
#endif  // CC_LAYERS_TEXTURE_LAYER_H_
