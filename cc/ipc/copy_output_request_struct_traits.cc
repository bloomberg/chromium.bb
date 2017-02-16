// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/ipc/copy_output_request_struct_traits.h"

#include "base/callback_helpers.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace {

// When we're sending a CopyOutputRequest, we keep the result_callback_ in a
// CopyOutputResultSenderImpl and send a CopyOutputResultSenderPtr to the other
// process. When SendResult is called, we run the stored result_callback_.
class CopyOutputResultSenderImpl : public cc::mojom::CopyOutputResultSender {
 public:
  CopyOutputResultSenderImpl(
      cc::CopyOutputRequest::CopyOutputRequestCallback result_callback)
      : result_callback_(result_callback) {
    DCHECK(result_callback_);
  }

  ~CopyOutputResultSenderImpl() override {
    if (result_callback_)
      result_callback_.Run(cc::CopyOutputResult::CreateEmptyResult());
  }

  // mojom::TextureMailboxReleaser implementation:
  void SendResult(std::unique_ptr<cc::CopyOutputResult> result) override {
    if (!result_callback_)
      return;
    base::ResetAndReturn(&result_callback_).Run(std::move(result));
  }

 private:
  cc::CopyOutputRequest::CopyOutputRequestCallback result_callback_;
};

void SendResult(cc::mojom::CopyOutputResultSenderPtr ptr,
                std::unique_ptr<cc::CopyOutputResult> result) {
  ptr->SendResult(std::move(result));
}

}  // namespace

namespace mojo {

// static
cc::mojom::CopyOutputResultSenderPtr
StructTraits<cc::mojom::CopyOutputRequestDataView,
             std::unique_ptr<cc::CopyOutputRequest>>::
    result_sender(const std::unique_ptr<cc::CopyOutputRequest>& request) {
  cc::mojom::CopyOutputResultSenderPtr result_sender;
  auto impl = base::MakeUnique<CopyOutputResultSenderImpl>(
      std::move(request->result_callback_));
  MakeStrongBinding(std::move(impl), MakeRequest(&result_sender));
  return result_sender;
}

// static
bool StructTraits<cc::mojom::CopyOutputRequestDataView,
                  std::unique_ptr<cc::CopyOutputRequest>>::
    Read(cc::mojom::CopyOutputRequestDataView data,
         std::unique_ptr<cc::CopyOutputRequest>* out_p) {
  auto request = cc::CopyOutputRequest::CreateEmptyRequest();

  request->force_bitmap_result_ = data.force_bitmap_result();

  if (!data.ReadSource(&request->source_))
    return false;

  if (!data.ReadArea(&request->area_))
    return false;

  if (!data.ReadTextureMailbox(&request->texture_mailbox_))
    return false;

  auto result_sender =
      data.TakeResultSender<cc::mojom::CopyOutputResultSenderPtr>();
  request->result_callback_ =
      base::Bind(SendResult, base::Passed(&result_sender));

  *out_p = std::move(request);

  return true;
}

}  // namespace mojo
