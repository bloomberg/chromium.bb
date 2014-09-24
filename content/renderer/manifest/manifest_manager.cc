// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/manifest/manifest_manager.h"

#include "base/bind.h"
#include "base/strings/nullable_string16.h"
#include "content/common/manifest_manager_messages.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/fetchers/manifest_fetcher.h"
#include "content/renderer/manifest/manifest_parser.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"

namespace content {

ManifestManager::ManifestManager(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      may_have_manifest_(false),
      manifest_dirty_(true) {
}

ManifestManager::~ManifestManager() {
  if (fetcher_)
    fetcher_->Cancel();

  // Consumers in the browser process will not receive this message but they
  // will be aware of the RenderFrame dying and should act on that. Consumers
  // in the renderer process should be correctly notified.
  ResolveCallbacks(ResolveStateFailure);
}

bool ManifestManager::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP(ManifestManager, message)
    IPC_MESSAGE_HANDLER(ManifestManagerMsg_RequestManifest, OnRequestManifest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void ManifestManager::OnRequestManifest(int request_id) {
  GetManifest(base::Bind(&ManifestManager::OnRequestManifestComplete,
                         base::Unretained(this), request_id));
}

void ManifestManager::OnRequestManifestComplete(
    int request_id, const Manifest& manifest) {
  // When sent via IPC, the Manifest must follow certain security rules.
  Manifest ipc_manifest = manifest;
  ipc_manifest.name = base::NullableString16(
      ipc_manifest.name.string().substr(0, Manifest::kMaxIPCStringLength),
      ipc_manifest.name.is_null());
  ipc_manifest.short_name = base::NullableString16(
        ipc_manifest.short_name.string().substr(0,
                                                Manifest::kMaxIPCStringLength),
        ipc_manifest.short_name.is_null());
  for (size_t i = 0; i < ipc_manifest.icons.size(); ++i) {
    ipc_manifest.icons[i].type = base::NullableString16(
        ipc_manifest.icons[i].type.string().substr(
            0, Manifest::kMaxIPCStringLength),
        ipc_manifest.icons[i].type.is_null());
  }

  Send(new ManifestManagerHostMsg_RequestManifestResponse(
      routing_id(), request_id, ipc_manifest));
}

void ManifestManager::GetManifest(const GetManifestCallback& callback) {
  if (!may_have_manifest_) {
    callback.Run(Manifest());
    return;
  }

  if (!manifest_dirty_) {
    callback.Run(manifest_);
    return;
  }

  pending_callbacks_.push_back(callback);

  // Just wait for the running call to be done if there are other callbacks.
  if (pending_callbacks_.size() > 1)
    return;

  FetchManifest();
}

void ManifestManager::DidChangeManifest() {
  may_have_manifest_ = true;
  manifest_dirty_ = true;
}

void ManifestManager::FetchManifest() {
  GURL url(render_frame()->GetWebFrame()->document().manifestURL());

  if (url.is_empty()) {
    ResolveCallbacks(ResolveStateFailure);
    return;
  }

  fetcher_.reset(new ManifestFetcher(url));

  // TODO(mlamouri,kenneth): this is not yet taking into account manifest-src
  // CSP rule, see http://crbug.com/409996.
  fetcher_->Start(render_frame()->GetWebFrame(),
                  base::Bind(&ManifestManager::OnManifestFetchComplete,
                             base::Unretained(this),
                             render_frame()->GetWebFrame()->document().url()));
}

void ManifestManager::OnManifestFetchComplete(
    const GURL& document_url,
    const blink::WebURLResponse& response,
    const std::string& data) {
  if (response.isNull() && data.empty()) {
    ResolveCallbacks(ResolveStateFailure);
    return;
  }

  manifest_ = ManifestParser::Parse(data, response.url(), document_url);

  fetcher_.reset();
  ResolveCallbacks(ResolveStateSuccess);
}

void ManifestManager::ResolveCallbacks(ResolveState state) {
  if (state == ResolveStateFailure)
    manifest_ = Manifest();

  manifest_dirty_ = state != ResolveStateSuccess;

  Manifest manifest = manifest_;
  std::list<GetManifestCallback> callbacks = pending_callbacks_;

  pending_callbacks_.clear();

  for (std::list<GetManifestCallback>::const_iterator it = callbacks.begin();
       it != callbacks.end(); ++it) {
    it->Run(manifest);
  }
}

} // namespace content
