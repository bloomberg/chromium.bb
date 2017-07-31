// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_WORKER_CONTENT_SETTING_INSTANCE_H_
#define CONTENT_BROWSER_SHARED_WORKER_CONTENT_SETTING_INSTANCE_H_

#include "base/callback.h"
#include "base/strings/string16.h"
#include "content/browser/shared_worker/shared_worker_host.h"
#include "content/common/content_export.h"
#include "content/public/browser/resource_context.h"
#include "third_party/WebKit/public/web/shared_worker_content_settings_proxy.mojom.h"
#include "url/origin.h"

namespace content {

// Passes the content settings information to the renderer counterpart through
// Mojo connection.
// Created on SharedWorker::Start() and connected to the counterpart
// at the moment.
// Kept alive while the connection is held in the renderer.
class CONTENT_EXPORT SharedWorkerContentSettingsProxyImpl
    : NON_EXPORTED_BASE(public blink::mojom::SharedWorkerContentSettingsProxy) {
 public:
  ~SharedWorkerContentSettingsProxyImpl() override;

  // Creates a new SharedWorkerContentSettingsProxyImpl and
  // binds it to |request|.
  static void Create(
      base::WeakPtr<SharedWorkerHost> host,
      blink::mojom::SharedWorkerContentSettingsProxyRequest request);

  // blink::mojom::SharedWorkerContentSettingsProxy implementation.
  void AllowIndexedDB(const url::Origin& origin,
                      const base::string16& name,
                      AllowIndexedDBCallback callback) override;
  void RequestFileSystemAccessSync(
      const url::Origin& origin,
      RequestFileSystemAccessSyncCallback callback) override;

 private:
  explicit SharedWorkerContentSettingsProxyImpl(
      base::WeakPtr<SharedWorkerHost> host);

  const base::WeakPtr<SharedWorkerHost> host_;

  DISALLOW_COPY_AND_ASSIGN(SharedWorkerContentSettingsProxyImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_WORKER_CONTENT_SETTING_INSTANCE_H_
