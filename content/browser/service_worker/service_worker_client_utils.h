// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CLIENT_UTILS_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CLIENT_UTILS_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "third_party/WebKit/public/mojom/service_worker/service_worker_client.mojom.h"
#include "ui/base/mojo/window_open_disposition.mojom.h"

class GURL;

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerProviderHost;
class ServiceWorkerVersion;

namespace service_worker_client_utils {

using NavigationCallback = base::OnceCallback<void(
    ServiceWorkerStatusCode status,
    blink::mojom::ServiceWorkerClientInfoPtr client_info)>;
using ClientCallback = base::OnceCallback<void(
    blink::mojom::ServiceWorkerClientInfoPtr client_info)>;
using ServiceWorkerClientPtrs =
    std::vector<blink::mojom::ServiceWorkerClientInfoPtr>;
using ClientsCallback =
    base::OnceCallback<void(std::unique_ptr<ServiceWorkerClientPtrs> clients)>;

// Focuses the window client associated with |provider_host|. |callback| is
// called with the client information on completion.
void FocusWindowClient(ServiceWorkerProviderHost* provider_host,
                       ClientCallback callback);

// Opens a new window and navigates it to |url|. |callback| is called with the
// window's client information on completion.
void OpenWindow(const GURL& url,
                const GURL& script_url,
                int worker_process_id,
                const base::WeakPtr<ServiceWorkerContextCore>& context,
                WindowOpenDisposition disposition,
                NavigationCallback callback);

// Navigates the client specified by |process_id| and |frame_id| to |url|.
// |callback| is called with the client information on completion.
void NavigateClient(const GURL& url,
                    const GURL& script_url,
                    int process_id,
                    int frame_id,
                    const base::WeakPtr<ServiceWorkerContextCore>& context,
                    NavigationCallback callback);

// Gets the client specified by |provider_host|. |callback| is called with the
// client information on completion.
void GetClient(const ServiceWorkerProviderHost* provider_host,
               ClientCallback callback);

// Collects clients matched with |options|. |callback| is called with the client
// information sorted in MRU order (most recently focused order) on completion.
void GetClients(const base::WeakPtr<ServiceWorkerVersion>& controller,
                blink::mojom::ServiceWorkerClientQueryOptionsPtr options,
                ClientsCallback callback);

// Finds the provider host for |origin| in |context| then uses
// |render_process_id| and |render_process_host| to create a relevant
// blink::mojom::ServiceWorkerClientInfo struct and calls |callback| with it.
// Must be called on the IO thread.
void DidNavigate(const base::WeakPtr<ServiceWorkerContextCore>& context,
                 const GURL& origin,
                 NavigationCallback callback,
                 int render_process_id,
                 int render_frame_id);

}  // namespace service_worker_client_utils

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CLIENT_UTILS_H_
