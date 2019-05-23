// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/mock_vendor_tag_ops.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/synchronization/waitable_event.h"

namespace media {
namespace unittest_internal {

MockVendorTagOps::MockVendorTagOps()
    : mock_vendor_tag_ops_thread_("MockVendorTagOpsThread"), binding_(this) {
  CHECK(mock_vendor_tag_ops_thread_.Start());
}

MockVendorTagOps::~MockVendorTagOps() {
  mock_vendor_tag_ops_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&MockVendorTagOps::CloseBindingOnThread,
                                base::Unretained(this)));
  mock_vendor_tag_ops_thread_.Stop();
}

void MockVendorTagOps::Bind(cros::mojom::VendorTagOpsRequest request) {
  base::WaitableEvent done(base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  cros::mojom::CameraModulePtrInfo ptr_info;
  mock_vendor_tag_ops_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&MockVendorTagOps::BindOnThread, base::Unretained(this),
                     base::Unretained(&done), std::move(request)));
  done.Wait();
}

void MockVendorTagOps::GetTagCount(GetTagCountCallback callback) {
  std::move(callback).Run(DoGetTagCount());
}

void MockVendorTagOps::GetAllTags(GetAllTagsCallback callback) {
  std::move(callback).Run(DoGetAllTags());
}

void MockVendorTagOps::GetSectionName(uint32_t tag,
                                      GetSectionNameCallback callback) {
  std::move(callback).Run(DoGetSectionName(tag));
}

void MockVendorTagOps::GetTagName(uint32_t tag, GetTagNameCallback callback) {
  std::move(callback).Run(DoGetTagName(tag));
}

void MockVendorTagOps::CloseBindingOnThread() {
  if (binding_.is_bound()) {
    binding_.Close();
  }
}

void MockVendorTagOps::BindOnThread(base::WaitableEvent* done,
                                    cros::mojom::VendorTagOpsRequest request) {
  binding_.Bind(std::move(request));
  done->Signal();
}

}  // namespace unittest_internal
}  // namespace media
