// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media_router/media_router_webui_message_handler.h"

#include "base/bind.h"
#include "chrome/browser/ui/webui/media_router/media_router_ui.h"
#include "content/public/browser/web_ui.h"

namespace media_router {

namespace {

// Message names.
const char kGetInitialSettings[] = "getInitialSettings";
const char kCreateRoute[] = "requestRoute";
const char kActOnIssue[] = "actOnIssue";
const char kCloseRoute[] = "closeRoute";
const char kCloseDialog[] = "closeDialog";

// TODO(imcheng): Define JS function names here.

}  // namespace

MediaRouterWebUIMessageHandler::MediaRouterWebUIMessageHandler()
    : dialog_closing_(false) {
}

MediaRouterWebUIMessageHandler::~MediaRouterWebUIMessageHandler() {
}

void MediaRouterWebUIMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      kGetInitialSettings,
      base::Bind(&MediaRouterWebUIMessageHandler::OnGetInitialSettings,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kCreateRoute,
      base::Bind(&MediaRouterWebUIMessageHandler::OnCreateRoute,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kActOnIssue,
      base::Bind(&MediaRouterWebUIMessageHandler::OnActOnIssue,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kCloseRoute,
      base::Bind(&MediaRouterWebUIMessageHandler::OnCloseRoute,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kCloseDialog,
      base::Bind(&MediaRouterWebUIMessageHandler::OnCloseDialog,
                 base::Unretained(this)));
}

void MediaRouterWebUIMessageHandler::OnGetInitialSettings(
    const base::ListValue* args) {
  // TODO(imcheng): Implement.
  NOTIMPLEMENTED();
}

void MediaRouterWebUIMessageHandler::OnCreateRoute(
    const base::ListValue* args) {
  // TODO(imcheng): Implement.
  NOTIMPLEMENTED();
}

void MediaRouterWebUIMessageHandler::OnActOnIssue(
    const base::ListValue* args) {
  // TODO(imcheng): Implement.
  NOTIMPLEMENTED();
}

void MediaRouterWebUIMessageHandler::OnCloseRoute(
    const base::ListValue* args) {
  // TODO(imcheng): Implement.
  NOTIMPLEMENTED();
}

void MediaRouterWebUIMessageHandler::OnCloseDialog(
    const base::ListValue* args) {
  CHECK(!dialog_closing_);
  dialog_closing_ = true;
  GetMediaRouterUI()->Close();
}

MediaRouterUI* MediaRouterWebUIMessageHandler::GetMediaRouterUI() const {
  MediaRouterUI* media_router_ui =
      static_cast<MediaRouterUI*>(web_ui()->GetController());
  DCHECK(media_router_ui);
  return media_router_ui;
}

}  // namespace media_router

