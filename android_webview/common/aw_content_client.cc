// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/aw_content_client.h"

#include "base/basictypes.h"
#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/user_agent.h"
#include "ipc/ipc_message.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

std::string GetProduct() {
  // "Chrome/XX.0.0.0" identifies that this WebView is derived from the
  // corresponding Chromium version XX.
  // TODO(torne): Use chrome/VERSION file. See http://crbug.com/297522
  return "Chrome/38.0.0.0";
}

}

namespace android_webview {

std::string GetUserAgent() {
  // "Version/4.0" had been hardcoded in the legacy WebView.
  std::string product = "Version/4.0 " + GetProduct();
  if (CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kUseMobileUserAgent)) {
    product += " Mobile";
  }
  return content::BuildUserAgentFromProduct(product);
}

std::string AwContentClient::GetProduct() const {
  return ::GetProduct();
}

std::string AwContentClient::GetUserAgent() const {
  return android_webview::GetUserAgent();
}

base::string16 AwContentClient::GetLocalizedString(int message_id) const {
  // TODO(boliu): Used only by WebKit, so only bundle those resources for
  // Android WebView.
  return l10n_util::GetStringUTF16(message_id);
}

base::StringPiece AwContentClient::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) const {
  // TODO(boliu): Used only by WebKit, so only bundle those resources for
  // Android WebView.
  return ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

bool AwContentClient::CanSendWhileSwappedOut(const IPC::Message* message) {
  // For legacy API support we perform a few browser -> renderer synchronous IPC
  // messages that block the browser. However, the synchronous IPC replies might
  // be dropped by the renderer during a swap out, deadlocking the browser.
  // Because of this we should never drop any synchronous IPC replies.
  return message->type() == IPC_REPLY_ID;
}

}  // namespace android_webview
