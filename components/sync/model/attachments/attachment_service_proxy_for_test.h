// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_ATTACHMENTS_ATTACHMENT_SERVICE_PROXY_FOR_TEST_H_
#define COMPONENTS_SYNC_MODEL_ATTACHMENTS_ATTACHMENT_SERVICE_PROXY_FOR_TEST_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/model/attachments/attachment_service_proxy.h"

namespace syncer {

// An self-contained AttachmentServiceProxy to reduce boilerplate code in tests.
//
// Constructs and owns an AttachmentService suitable for use in tests.
// NOTE: This class does not require the current thread to have a MessageLoop,
// however all methods will effectively become no-op stubs in that case.
class AttachmentServiceProxyForTest : public AttachmentServiceProxy {
 public:
  static AttachmentServiceProxy Create();
  ~AttachmentServiceProxyForTest() override;

 private:
  // A Core that owns the wrapped AttachmentService.
  class OwningCore : public AttachmentServiceProxy::Core {
   public:
    OwningCore(std::unique_ptr<AttachmentService>,
               std::unique_ptr<base::WeakPtrFactory<AttachmentService>>
                   weak_ptr_factory);

   private:
    ~OwningCore() override;

    std::unique_ptr<AttachmentService> wrapped_;
    // WeakPtrFactory for wrapped_. See Create() for why this is a unique_ptr.
    std::unique_ptr<base::WeakPtrFactory<AttachmentService>> weak_ptr_factory_;

    DISALLOW_COPY_AND_ASSIGN(OwningCore);
  };

  AttachmentServiceProxyForTest(
      const scoped_refptr<base::SequencedTaskRunner>& wrapped_task_runner,
      const scoped_refptr<Core>& core);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_ATTACHMENTS_ATTACHMENT_SERVICE_PROXY_FOR_TEST_H_
