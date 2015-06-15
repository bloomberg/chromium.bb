// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media_router/media_router_webui_message_handler.h"

#include "base/bind.h"
#include "chrome/browser/ui/webui/media_router/media_router_ui.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"

namespace media_router {

namespace {

// Message names.
const char kGetInitialSettings[] = "getInitialSettings";
const char kCreateRoute[] = "requestRoute";
const char kActOnIssue[] = "actOnIssue";
const char kCloseRoute[] = "closeRoute";
const char kCloseDialog[] = "closeDialog";

// JS function names.
const char kSetInitialSettings[] = "media_router.setInitialSettings";
const char kAddRoute[] = "media_router.ui.addRoute";
const char kSetSinkList[] = "media_router.ui.setSinkList";
const char kSetRouteList[] = "media_router.ui.setRouteList";
const char kSetCastModeList[] = "media_router.ui.setCastModeList";

scoped_ptr<base::ListValue> SinksToValue(
    const std::vector<MediaSinkWithCastModes>& sinks) {
  scoped_ptr<base::ListValue> value(new base::ListValue);

  for (const MediaSinkWithCastModes& sink_with_cast_modes : sinks) {
    scoped_ptr<base::DictionaryValue> sink_val(new base::DictionaryValue);

    const MediaSink& sink = sink_with_cast_modes.sink;
    sink_val->SetString("id", sink.id());
    sink_val->SetString("name", sink.name());

    scoped_ptr<base::ListValue> cast_modes_val(new base::ListValue);
    for (MediaCastMode cast_mode : sink_with_cast_modes.cast_modes)
      cast_modes_val->AppendInteger(cast_mode);
    sink_val->Set("castModes", cast_modes_val.Pass());

    value->Append(sink_val.release());
  }

  return value.Pass();
}

scoped_ptr<base::DictionaryValue> RouteToValue(const MediaRoute& route) {
  scoped_ptr<base::DictionaryValue> dictionary(new base::DictionaryValue);

  dictionary->SetString("id", route.media_route_id());
  dictionary->SetString("sinkId", route.media_sink().id());
  dictionary->SetString("title", route.description());
  dictionary->SetBoolean("isLocal", route.is_local());

  return dictionary.Pass();
}

scoped_ptr<base::ListValue> RoutesToValue(
    const std::vector<MediaRoute>& routes) {
  scoped_ptr<base::ListValue> value(new base::ListValue);

  for (const MediaRoute& route : routes) {
    scoped_ptr<base::DictionaryValue> route_val(RouteToValue(route));
    value->Append(route_val.release());
  }

  return value.Pass();
}

scoped_ptr<base::ListValue> CastModesToValue(const CastModeSet& cast_modes,
                                             const std::string& source_host) {
  scoped_ptr<base::ListValue> value(new base::ListValue);

  for (const MediaCastMode& cast_mode : cast_modes) {
    scoped_ptr<base::DictionaryValue> cast_mode_val(new base::DictionaryValue);
    cast_mode_val->SetInteger("type", cast_mode);
    cast_mode_val->SetString("title",
                             MediaCastModeToTitle(cast_mode, source_host));
    cast_mode_val->SetString(
        "description", MediaCastModeToDescription(cast_mode, source_host));
    value->Append(cast_mode_val.release());
  }

  return value.Pass();
}

}  // namespace

MediaRouterWebUIMessageHandler::MediaRouterWebUIMessageHandler()
    : dialog_closing_(false) {
}

MediaRouterWebUIMessageHandler::~MediaRouterWebUIMessageHandler() {
}

void MediaRouterWebUIMessageHandler::UpdateSinks(
    const std::vector<MediaSinkWithCastModes>& sinks) {
  DVLOG(2) << "UpdateSinks";
  scoped_ptr<base::ListValue> sinks_val(SinksToValue(sinks));
  web_ui()->CallJavascriptFunction(kSetSinkList, *sinks_val);
}

void MediaRouterWebUIMessageHandler::UpdateRoutes(
    const std::vector<MediaRoute>& routes) {
  DVLOG(2) << "UpdateRoutes";
  scoped_ptr<base::ListValue> routes_val(RoutesToValue(routes));
  web_ui()->CallJavascriptFunction(kSetRouteList, *routes_val);
}

void MediaRouterWebUIMessageHandler::UpdateCastModes(
    const CastModeSet& cast_modes,
    const std::string& source_host) {
  DVLOG(2) << "UpdateCastModes";
  scoped_ptr<base::ListValue> cast_modes_val(
      CastModesToValue(cast_modes, source_host));
  web_ui()->CallJavascriptFunction(kSetCastModeList, *cast_modes_val);
}

void MediaRouterWebUIMessageHandler::AddRoute(const MediaRoute& route) {
  DVLOG(2) << "AddRoute";

  scoped_ptr<base::DictionaryValue> route_value(RouteToValue(route));
  web_ui()->CallJavascriptFunction(kAddRoute, *route_value);
}

void MediaRouterWebUIMessageHandler::UpdateIssue(const Issue* issue) {
  DVLOG(2) << "UpdateIssue";
  // TODO(imcheng): Implement conversion from Issue to dictionary object
  // (crbug.com/464216).
  NOTIMPLEMENTED();
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
  MediaRouterUI* media_router_ui = GetMediaRouterUI();

  base::DictionaryValue initial_settings;

  initial_settings.SetString("headerText",
                             media_router_ui->GetInitialHeaderText());

  scoped_ptr<base::ListValue> sinks(SinksToValue(media_router_ui->sinks()));
  initial_settings.Set("sinks", sinks.release());

  scoped_ptr<base::ListValue> routes(RoutesToValue(media_router_ui->routes()));
  initial_settings.Set("routes", routes.release());

  scoped_ptr<base::ListValue> cast_modes(CastModesToValue(
      media_router_ui->cast_modes(), media_router_ui->GetFrameURLHost()));
  initial_settings.Set("castModes", cast_modes.release());

  web_ui()->CallJavascriptFunction(kSetInitialSettings, initial_settings);
  media_router_ui->UIInitialized();
}

void MediaRouterWebUIMessageHandler::OnCreateRoute(
    const base::ListValue* args) {
  std::string sink_id;
  int cast_mode_num;
  if (!args->GetString(0, &sink_id) || !args->GetInteger(1, &cast_mode_num)) {
    LOG(ERROR) << "Unable to extract args.";
    return;
  }

  if (sink_id.empty()) {
    LOG(ERROR) << "Media Route Provider Manager did not respond with a "
               << "valid sink ID. Aborting.";
    return;
  }

  MediaRouterUI* media_router_ui =
      static_cast<MediaRouterUI*>(web_ui()->GetController());
  if (media_router_ui->has_pending_route_request()) {
    LOG(ERROR) << "UI already has pending route request. Ignoring.";
    return;
  }

  DVLOG(2) << "sink id: " << sink_id << ", cast mode: " << cast_mode_num;

  // TODO(haibinlu): Pass additional parameters into the CreateRoute request,
  // e.g. low-fps-mirror, user-override. (crbug.com/490364)
  bool success = false;
  if (IsValidCastModeNum(cast_mode_num)) {
    // User explicitly selected cast mode.
    DVLOG(2) << "Cast mode override: " << cast_mode_num;
    success = media_router_ui->CreateRouteWithCastModeOverride(
        sink_id, static_cast<MediaCastMode>(cast_mode_num));
  } else {
    success = media_router_ui->CreateRoute(sink_id);
  }

  // TODO(imcheng): Display error in UI. (crbug.com/490372)
  if (!success)
    LOG(ERROR) << "Error initiating route request.";
}

void MediaRouterWebUIMessageHandler::OnActOnIssue(
    const base::ListValue* args) {
  // TODO(imcheng): Implement (crbug.com/464216).
  NOTIMPLEMENTED();
}

void MediaRouterWebUIMessageHandler::OnCloseRoute(
    const base::ListValue* args) {
  DVLOG(2) << "OnCloseRoute";
  std::string route_id;
  if (!args->GetString(0, &route_id)) {
    LOG(ERROR) << "Unable to extract args.";
    return;
  }
  GetMediaRouterUI()->CloseRoute(route_id);
}

void MediaRouterWebUIMessageHandler::OnCloseDialog(
    const base::ListValue* args) {
  if (dialog_closing_)
    return;

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

