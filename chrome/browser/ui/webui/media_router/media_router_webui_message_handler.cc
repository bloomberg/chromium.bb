// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media_router/media_router_webui_message_handler.h"

#include <string>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ui/webui/media_router/media_router_ui.h"

namespace media_router {

namespace {

const char kHelpPageUrlPrefix[] =
    "https://support.google.com/chromecast/answer/%d";

// Message names.
const char kRequestInitialData[] = "requestInitialData";
const char kCreateRoute[] = "requestRoute";
const char kActOnIssue[] = "actOnIssue";
const char kCloseRoute[] = "closeRoute";
const char kCloseDialog[] = "closeDialog";

// JS function names.
const char kSetInitialData[] = "media_router.ui.setInitialData";
const char kAddRoute[] = "media_router.ui.addRoute";
const char kSetIssue[] = "media_router.ui.setIssue";
const char kSetSinkList[] = "media_router.ui.setSinkList";
const char kSetRouteList[] = "media_router.ui.setRouteList";
const char kSetCastModeList[] = "media_router.ui.setCastModeList";
const char kWindowOpen[] = "window.open";

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
  dictionary->SetString("customControllerPath", route.custom_controller_path());

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
    cast_mode_val->SetString("host", source_host);
    cast_mode_val->SetString(
        "description", MediaCastModeToDescription(cast_mode, source_host));
    value->Append(cast_mode_val.release());
  }

  return value.Pass();
}

// Returns an Issue dictionary created from |issue| that can be used in WebUI.
scoped_ptr<base::DictionaryValue> IssueToValue(const Issue& issue) {
  scoped_ptr<base::DictionaryValue> dictionary(new base::DictionaryValue);
  dictionary->SetString("id", issue.id());
  dictionary->SetString("title", issue.title());
  dictionary->SetString("message", issue.message());
  dictionary->SetInteger("defaultActionType", issue.default_action().type());
  if (!issue.secondary_actions().empty()) {
    dictionary->SetInteger("secondaryActionType",
                           issue.secondary_actions().begin()->type());
  }
  if (!issue.route_id().empty())
    dictionary->SetString("routeId", issue.route_id());
  dictionary->SetBoolean("isBlocking", issue.is_blocking());

  return dictionary.Pass();
}

bool IsValidIssueActionTypeNum(int issue_action_type_num) {
  return issue_action_type_num >= 0 &&
         issue_action_type_num < IssueAction::TYPE_MAX;
}

// Composes a "learn more" URL. The URL depends on template arguments in |args|.
// Returns an empty string if |args| is invalid.
std::string GetLearnMoreUrl(const base::DictionaryValue* args) {
  // TODO(imcheng): The template arguments for determining the learn more URL
  // should come from the Issue object in the browser, not from WebUI.
  int help_page_id = -1;
  if (!args->GetInteger("helpPageId", &help_page_id) || help_page_id < 0) {
    DVLOG(1) << "Invalid help page id.";
    return std::string();
  }

  std::string help_url = base::StringPrintf(kHelpPageUrlPrefix, help_page_id);
  if (!GURL(help_url).is_valid()) {
    DVLOG(1) << "Error: URL is invalid and cannot be opened.";
    return std::string();
  }
  return help_url;
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
  if (issue) {
    scoped_ptr<base::DictionaryValue> issue_val(IssueToValue(*issue));
    web_ui()->CallJavascriptFunction(kSetIssue, *issue_val);
  } else {
    // Clears the issue in the WebUI.
    web_ui()->CallJavascriptFunction(kSetIssue);
  }
}

void MediaRouterWebUIMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      kRequestInitialData,
      base::Bind(&MediaRouterWebUIMessageHandler::OnRequestInitialData,
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

void MediaRouterWebUIMessageHandler::OnRequestInitialData(
    const base::ListValue* args) {
  DVLOG(1) << "OnRequestInitialData";
  MediaRouterUI* media_router_ui = GetMediaRouterUI();

  base::DictionaryValue initial_data;

  initial_data.SetString("headerText", media_router_ui->GetInitialHeaderText());
  initial_data.SetString("headerTextTooltip",
      media_router_ui->GetInitialHeaderTextTooltip());

  scoped_ptr<base::ListValue> sinks(SinksToValue(media_router_ui->sinks()));
  initial_data.Set("sinks", sinks.release());

  scoped_ptr<base::ListValue> routes(RoutesToValue(media_router_ui->routes()));
  initial_data.Set("routes", routes.release());

  scoped_ptr<base::ListValue> cast_modes(CastModesToValue(
      media_router_ui->cast_modes(),
      media_router_ui->GetFrameURLHost()));
  initial_data.Set("castModes", cast_modes.release());

  initial_data.SetString("routeProviderExtensionId",
                         GetMediaRouterUI()->GetRouteProviderExtensionId());

  web_ui()->CallJavascriptFunction(kSetInitialData, initial_data);
  media_router_ui->UIInitialized();
}

void MediaRouterWebUIMessageHandler::OnCreateRoute(
    const base::ListValue* args) {
  DVLOG(1) << "OnCreateRoute";
  const base::DictionaryValue* args_dict = nullptr;
  std::string sink_id;
  int cast_mode_num = -1;
  if (!args->GetDictionary(0, &args_dict) ||
      !args_dict->GetString("sinkId", &sink_id) ||
      !args_dict->GetInteger("selectedCastMode", &cast_mode_num)) {
    DVLOG(1) << "Unable to extract args.";
    return;
  }

  if (sink_id.empty()) {
    DVLOG(1) << "Media Route Provider Manager did not respond with a "
             << "valid sink ID. Aborting.";
    return;
  }

  MediaRouterUI* media_router_ui =
      static_cast<MediaRouterUI*>(web_ui()->GetController());
  if (media_router_ui->has_pending_route_request()) {
    DVLOG(1) << "UI already has pending route request. Ignoring.";
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
    DVLOG(1) << "Error initiating route request.";
}

void MediaRouterWebUIMessageHandler::OnActOnIssue(
    const base::ListValue* args) {
  DVLOG(1) << "OnActOnIssue";
  const base::DictionaryValue* args_dict = nullptr;
  Issue::Id issue_id;
  int action_type_num = -1;
  if (!args->GetDictionary(0, &args_dict) ||
      !args_dict->GetString("issueId", &issue_id) ||
      !args_dict->GetInteger("actionType", &action_type_num)) {
    DVLOG(1) << "Unable to extract args.";
    return;
  }
  if (!IsValidIssueActionTypeNum(action_type_num)) {
    DVLOG(1) << "Invalid action type: " << action_type_num;
    return;
  }
  IssueAction::Type action_type =
      static_cast<IssueAction::Type>(action_type_num);
  if (ActOnIssueType(action_type, args_dict))
    DVLOG(1) << "ActOnIssueType failed for Issue ID " << issue_id;
  GetMediaRouterUI()->ClearIssue(issue_id);
}

void MediaRouterWebUIMessageHandler::OnCloseRoute(
    const base::ListValue* args) {
  DVLOG(1) << "OnCloseRoute";
  const base::DictionaryValue* args_dict = nullptr;
  std::string route_id;
  if (!args->GetDictionary(0, &args_dict) ||
      !args_dict->GetString("routeId", &route_id)) {
    DVLOG(1) << "Unable to extract args.";
    return;
  }
  GetMediaRouterUI()->CloseRoute(route_id);
}

void MediaRouterWebUIMessageHandler::OnCloseDialog(
    const base::ListValue* args) {
  DVLOG(1) << "OnCloseDialog";
  if (dialog_closing_)
    return;

  dialog_closing_ = true;
  GetMediaRouterUI()->Close();
}

bool MediaRouterWebUIMessageHandler::ActOnIssueType(
    const IssueAction::Type& action_type,
    const base::DictionaryValue* args) {
  if (action_type == IssueAction::TYPE_LEARN_MORE) {
    std::string learn_more_url = GetLearnMoreUrl(args);
    if (learn_more_url.empty())
      return false;
    scoped_ptr<base::ListValue> open_args(new base::ListValue);
    open_args->AppendString(learn_more_url);
    web_ui()->CallJavascriptFunction(kWindowOpen, *open_args);
    return true;
  } else {
    // Do nothing; no other issue action types require any other action.
    return true;
  }
}

MediaRouterUI* MediaRouterWebUIMessageHandler::GetMediaRouterUI() {
  MediaRouterUI* media_router_ui =
      static_cast<MediaRouterUI*>(web_ui()->GetController());
  DCHECK(media_router_ui);
  return media_router_ui;
}

}  // namespace media_router
