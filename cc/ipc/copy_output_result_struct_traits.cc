// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/ipc/copy_output_result_struct_traits.h"

#include "mojo/public/cpp/bindings/strong_binding.h"

namespace {

// This class retains the release_callback_ of the CopyOutputResult that is
// being sent over mojo. A TextureMailboxReleaserPtr that talks to this impl
// object will be sent over mojo instead of the release_callback_ (which is not
// serializable). Once the client calls Release, the release_callback_ will be
// called. An object of this class will remain alive until the MessagePipe
// attached to it goes away (i.e. StrongBinding is used).
class TextureMailboxReleaserImpl : public cc::mojom::TextureMailboxReleaser {
 public:
  TextureMailboxReleaserImpl(
      std::unique_ptr<viz::SingleReleaseCallback> release_callback)
      : release_callback_(std::move(release_callback)) {
    DCHECK(release_callback_);
  }

  ~TextureMailboxReleaserImpl() override {
    // If the client fails to call Release, we should do it ourselves because
    // release_callback_ will fail if it's never called.
    if (release_callback_)
      release_callback_->Run(gpu::SyncToken(), true);
  }

  // mojom::TextureMailboxReleaser implementation:
  void Release(const gpu::SyncToken& sync_token, bool is_lost) override {
    if (!release_callback_)
      return;
    release_callback_->Run(sync_token, is_lost);
    release_callback_.reset();
  }

 private:
  std::unique_ptr<viz::SingleReleaseCallback> release_callback_;
};

void Release(cc::mojom::TextureMailboxReleaserPtr ptr,
             const gpu::SyncToken& sync_token,
             bool is_lost) {
  ptr->Release(sync_token, is_lost);
}

}  // namespace

namespace mojo {

// static
const SkBitmap& StructTraits<cc::mojom::CopyOutputResultDataView,
                             std::unique_ptr<viz::CopyOutputResult>>::
    bitmap(const std::unique_ptr<viz::CopyOutputResult>& result) {
  static SkBitmap* null_bitmap = new SkBitmap();
  if (!result->bitmap_)
    return *null_bitmap;
  return *result->bitmap_;
}

// static
cc::mojom::TextureMailboxReleaserPtr
StructTraits<cc::mojom::CopyOutputResultDataView,
             std::unique_ptr<viz::CopyOutputResult>>::
    releaser(const std::unique_ptr<viz::CopyOutputResult>& result) {
  if (!result->release_callback_)
    return {};
  cc::mojom::TextureMailboxReleaserPtr releaser;
  auto impl = std::make_unique<TextureMailboxReleaserImpl>(
      std::move(result->release_callback_));
  MakeStrongBinding(std::move(impl), MakeRequest(&releaser));
  return releaser;
}

// static
bool StructTraits<cc::mojom::CopyOutputResultDataView,
                  std::unique_ptr<viz::CopyOutputResult>>::
    Read(cc::mojom::CopyOutputResultDataView data,
         std::unique_ptr<viz::CopyOutputResult>* out_p) {
  // We first read into local variables and then call the appropriate
  // constructor of viz::CopyOutputResult.
  gfx::Size size;
  auto bitmap = std::make_unique<SkBitmap>();
  viz::TextureMailbox texture_mailbox;
  std::unique_ptr<viz::SingleReleaseCallback> release_callback;

  if (!data.ReadSize(&size))
    return false;

  if (!data.ReadBitmap(bitmap.get()))
    return false;

  if (!data.ReadTextureMailbox(&texture_mailbox))
    return false;

  auto releaser = data.TakeReleaser<cc::mojom::TextureMailboxReleaserPtr>();
  if (releaser) {
    // CopyOutputResult does not have a TextureMailboxReleaserPtr member.
    // We use base::Bind to turn TextureMailboxReleaser::Release into a
    // viz::ReleaseCallback.
    release_callback = viz::SingleReleaseCallback::Create(
        base::Bind(Release, base::Passed(&releaser)));
  }

  // Empty result.
  if (bitmap->isNull() && !texture_mailbox.IsTexture()) {
    *out_p = viz::CopyOutputResult::CreateEmptyResult();
    return true;
  }

  // Bitmap result.
  if (!bitmap->isNull()) {
    // We can't have both a bitmap and a texture.
    if (texture_mailbox.IsTexture())
      return false;
    *out_p = viz::CopyOutputResult::CreateBitmapResult(std::move(bitmap));
    return true;
  }

  // Texture result.
  DCHECK(texture_mailbox.IsTexture());
  if (size.IsEmpty())
    return false;
  if (!release_callback)
    return false;
  *out_p = viz::CopyOutputResult::CreateTextureResult(
      size, texture_mailbox, std::move(release_callback));
  return true;
}

}  // namespace mojo
