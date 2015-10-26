// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media_router/media_router_webui_message_handler.h"

#include <string>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/media/router/issue.h"
#include "chrome/browser/ui/webui/media_router/media_router_ui.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/common/constants.h"
#include "ui/base/l10n/l10n_util.h"

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
const char kReportSinkCount[] = "reportSinkCount";

// JS function names.
const char kSetInitialData[] = "media_router.ui.setInitialData";
const char kNotifyRouteCreationTimeout[] =
    "media_router.ui.onNotifyRouteCreationTimeout";
const char kOnCreateRouteResponseReceived[] =
    "media_router.ui.onCreateRouteResponseReceived";
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
    sink_val->SetInteger("iconType", sink.icon_type());

    scoped_ptr<base::ListValue> cast_modes_val(new base::ListValue);
    for (MediaCastMode cast_mode : sink_with_cast_modes.cast_modes)
      cast_modes_val->AppendInteger(cast_mode);
    sink_val->Set("castModes", cast_modes_val.Pass());

    value->Append(sink_val.release());
  }

  return value.Pass();
}

scoped_ptr<base::DictionaryValue> RouteToValue(
    const MediaRoute& route, const std::string& extension_id) {
  scoped_ptr<base::DictionaryValue> dictionary(new base::DictionaryValue);

  dictionary->SetString("id", route.media_route_id());
  dictionary->SetString("sinkId", route.media_sink_id());
  dictionary->SetString("description", route.description());
  dictionary->SetBoolean("isLocal", route.is_local());

  const std::string& custom_path = route.custom_controller_path();

  if (!custom_path.empty()) {
    std::string full_custom_controller_path = base::StringPrintf("%s://%s/%s",
        extensions::kExtensionScheme, extension_id.c_str(),
            custom_path.c_str());
    DCHECK(GURL(full_custom_controller_path).is_valid());
    dictionary->SetString("customControllerPath",
                          full_custom_controller_path);
  }

  return dictionary.Pass();
}

scoped_ptr<base::ListValue> RoutesToValue(
    const std::vector<MediaRoute>& routes, const std::string& extension_id) {
  scoped_ptr<base::ListValue> value(new base::ListValue);

  for (const MediaRoute& route : routes) {
    scoped_ptr<base::DictionaryValue> route_val(RouteToValue(route,
        extension_id));
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
    cast_mode_val->SetString(
        "description", MediaCastModeToDescription(cast_mode, source_host));
    cast_mode_val->SetString("host", source_host);
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

MediaRouterWebUIMessageHandler::MediaRouterWebUIMessageHandler(
    MediaRouterUI* media_router_ui)
    : dialog_closing_(false),
      media_router_ui_(media_router_ui) {
  DCHECK(media_router_ui_);
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
  scoped_ptr<base::ListValue> routes_val(RoutesToValue(routes,
      media_router_ui_->GetRouteProviderExtensionId()));
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

void MediaRouterWebUIMessageHandler::OnCreateRouteResponseReceived(
    const MediaSink::Id& sink_id,
    const MediaRoute* route) {
  DVLOG(2) << "OnCreateRouteResponseReceived";
  if (route) {
    scoped_ptr<base::DictionaryValue> route_value(RouteToValue(*route,
        media_router_ui_->GetRouteProviderExtensionId()));
    web_ui()->CallJavascriptFunction(kOnCreateRouteResponseReceived,
                                     base::StringValue(sink_id), *route_value);
    UMA_HISTOGRAM_BOOLEAN("MediaRouter.Ui.Action.StartLocalSessionSuccessful",
                          true);
  } else {
    web_ui()->CallJavascriptFunction(kOnCreateRouteResponseReceived,
                                     base::StringValue(sink_id),
                                     *base::Value::CreateNullValue());
    UMA_HISTOGRAM_BOOLEAN("MediaRouter.Ui.Action.StartLocalSessionSuccessful",
                          false);
  }
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

void MediaRouterWebUIMessageHandler::NotifyRouteCreationTimeout() {
  DVLOG(2) << "NotifyRouteCreationTimeout";
  web_ui()->CallJavascriptFunction(kNotifyRouteCreationTimeout);
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
  web_ui()->RegisterMessageCallback(
      kReportSinkCount,
      base::Bind(&MediaRouterWebUIMessageHandler::OnReportSinkCount,
                 base::Unretained(this)));
}

void MediaRouterWebUIMessageHandler::OnRequestInitialData(
    const base::ListValue* args) {
  DVLOG(1) << "OnRequestInitialData";
  base::DictionaryValue initial_data;

  // "No Cast devices found?" Chromecast help center page.
  initial_data.SetString("deviceMissingUrl",
      base::StringPrintf(kHelpPageUrlPrefix, 3249268));

  scoped_ptr<base::ListValue> sinks(SinksToValue(media_router_ui_->sinks()));
  initial_data.Set("sinks", sinks.release());

  scoped_ptr<base::ListValue> routes(RoutesToValue(media_router_ui_->routes(),
      media_router_ui_->GetRouteProviderExtensionId()));
  initial_data.Set("routes", routes.release());

  const std::set<MediaCastMode> cast_modes = media_router_ui_->cast_modes();
  scoped_ptr<base::ListValue> cast_modes_list(
      CastModesToValue(cast_modes, media_router_ui_->GetFrameURLHost()));
  initial_data.Set("castModes", cast_modes_list.release());
  if (!cast_modes.empty()) {
    initial_data.SetInteger("initialCastModeType",
                            GetPreferredCastMode(cast_modes));
  }

  web_ui()->CallJavascriptFunction(kSetInitialData, initial_data);
  media_router_ui_->UIInitialized();
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
    DVLOG(1) << "Media Route UI did not respond with a "
             << "valid sink ID. Aborting.";
    return;
  }

  if (!IsValidCastModeNum(cast_mode_num)) {
    DVLOG(1) << "Invalid cast mode: " << cast_mode_num << ". Aborting.";
    return;
  }

  MediaRouterUI* media_router_ui =
      static_cast<MediaRouterUI*>(web_ui()->GetController());
  if (media_router_ui->HasPendingRouteRequest()) {
    DVLOG(1) << "UI already has pending route request. Ignoring.";
    Issue issue(
        l10n_util::GetStringUTF8(IDS_MEDIA_ROUTER_ISSUE_PENDING_ROUTE),
        std::string(), IssueAction(IssueAction::TYPE_DISMISS),
        std::vector<IssueAction>(), std::string(), Issue::NOTIFICATION,
        false, std::string());
    media_router_ui_->AddIssue(issue);
    return;
  }

  DVLOG(2) << __FUNCTION__ << ": sink id: " << sink_id
           << ", cast mode: " << cast_mode_num;

  // TODO(haibinlu): Pass additional parameters into the CreateRoute request,
  // e.g. low-fps-mirror, user-override. (crbug.com/490364)
  if (!media_router_ui->CreateRoute(
          sink_id, static_cast<MediaCastMode>(cast_mode_num))) {
    // TODO(imcheng): Need to add an issue if failed to initiate a CreateRoute
    // request.
    DVLOG(1) << "Error initiating route request.";
  }
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
  media_router_ui_->ClearIssue(issue_id);
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
  media_router_ui_->CloseRoute(route_id);
}

void MediaRouterWebUIMessageHandler::OnCloseDialog(
    const base::ListValue* args) {
  DVLOG(1) << "OnCloseDialog";
  if (dialog_closing_)
    return;

  dialog_closing_ = true;
  media_router_ui_->Close();
}

void MediaRouterWebUIMessageHandler::OnReportSinkCount(
    const base::ListValue* args) {
  DVLOG(1) << "OnReportSinkCount";
  int sink_count;
  if (!args->GetInteger(0, &sink_count)) {
    DVLOG(1) << "Unable to extract args.";
    return;
  }
  UMA_HISTOGRAM_COUNTS_100("MediaRouter.Ui.Device.Count", sink_count);
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

}  // namespace media_router
