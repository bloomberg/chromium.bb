// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media_router/media_router_webui_message_handler.h"

#include <string>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/media/router/issue.h"
#include "chrome/browser/media/router/media_router_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/media_router/media_router_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "extensions/common/constants.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(GOOGLE_CHROME_BUILD)
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/signin/core/browser/signin_manager.h"
#endif  // defined(GOOGLE_CHROME_BUILD)

namespace media_router {

namespace {

const char kCastLearnMorePageUrl[] =
    "https://www.google.com/chrome/devices/chromecast/learn.html";
const char kHelpPageUrlPrefix[] =
    "https://support.google.com/chromecast/answer/%d";

// Message names.
const char kRequestInitialData[] = "requestInitialData";
const char kCreateRoute[] = "requestRoute";
const char kAcknowledgeFirstRunFlow[] = "acknowledgeFirstRunFlow";
const char kActOnIssue[] = "actOnIssue";
const char kCloseRoute[] = "closeRoute";
const char kJoinRoute[] = "joinRoute";
const char kCloseDialog[] = "closeDialog";
const char kReportClickedSinkIndex[] = "reportClickedSinkIndex";
const char kReportInitialAction[] = "reportInitialAction";
const char kReportInitialState[] = "reportInitialState";
const char kReportNavigateToView[] = "reportNavigateToView";
const char kReportRouteCreation[] = "reportRouteCreation";
const char kReportSelectedCastMode[] = "reportSelectedCastMode";
const char kReportSinkCount[] = "reportSinkCount";
const char kReportTimeToClickSink[] = "reportTimeToClickSink";
const char kReportTimeToInitialActionClose[] = "reportTimeToInitialActionClose";
const char kOnInitialDataReceived[] = "onInitialDataReceived";

// JS function names.
const char kSetInitialData[] = "media_router.ui.setInitialData";
const char kOnCreateRouteResponseReceived[] =
    "media_router.ui.onCreateRouteResponseReceived";
const char kSetFirstRunFlowData[] = "media_router.ui.setFirstRunFlowData";
const char kSetIssue[] = "media_router.ui.setIssue";
const char kSetSinkList[] = "media_router.ui.setSinkList";
const char kSetRouteList[] = "media_router.ui.setRouteList";
const char kSetCastModeList[] = "media_router.ui.setCastModeList";
const char kUpdateMaxHeight[] = "media_router.ui.updateMaxHeight";
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
    if (!sink.description().empty())
      sink_val->SetString("description", sink.description());
    if (!sink.domain().empty())
      sink_val->SetString("domain", sink.domain());

    int cast_mode_bits = 0;
    for (MediaCastMode cast_mode : sink_with_cast_modes.cast_modes)
      cast_mode_bits |= cast_mode;

    sink_val->SetInteger("castModes", cast_mode_bits);
    value->Append(sink_val.release());
  }

  return value;
}

scoped_ptr<base::DictionaryValue> RouteToValue(
    const MediaRoute& route, bool canJoin, const std::string& extension_id) {
  scoped_ptr<base::DictionaryValue> dictionary(new base::DictionaryValue);
  dictionary->SetString("id", route.media_route_id());
  dictionary->SetString("sinkId", route.media_sink_id());
  dictionary->SetString("description", route.description());
  dictionary->SetBoolean("isLocal", route.is_local());
  dictionary->SetBoolean("canJoin", canJoin);

  const std::string& custom_path = route.custom_controller_path();
  if (!custom_path.empty()) {
    std::string full_custom_controller_path = base::StringPrintf("%s://%s/%s",
        extensions::kExtensionScheme, extension_id.c_str(),
            custom_path.c_str());
    DCHECK(GURL(full_custom_controller_path).is_valid());
    dictionary->SetString("customControllerPath",
                          full_custom_controller_path);
  }

  return dictionary;
}

scoped_ptr<base::ListValue> RoutesToValue(
    const std::vector<MediaRoute>& routes,
    const std::vector<MediaRoute::Id>& joinable_route_ids,
    const std::string& extension_id) {
  scoped_ptr<base::ListValue> value(new base::ListValue);

  for (const MediaRoute& route : routes) {
    bool canJoin = ContainsValue(joinable_route_ids, route.media_route_id());
    scoped_ptr<base::DictionaryValue> route_val(RouteToValue(route,
        canJoin, extension_id));
    value->Append(route_val.release());
  }

  return value;
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

  return value;
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

  return dictionary;
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
    const std::vector<MediaRoute>& routes,
    const std::vector<MediaRoute::Id>& joinable_route_ids) {
  scoped_ptr<base::ListValue> routes_val(RoutesToValue(routes,
      joinable_route_ids,
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
    scoped_ptr<base::DictionaryValue> route_value(RouteToValue(*route, false,
        media_router_ui_->GetRouteProviderExtensionId()));
    web_ui()->CallJavascriptFunction(
        kOnCreateRouteResponseReceived,
        base::StringValue(sink_id), *route_value,
        base::FundamentalValue(route->for_display()));
  } else {
    web_ui()->CallJavascriptFunction(kOnCreateRouteResponseReceived,
                                     base::StringValue(sink_id),
                                     *base::Value::CreateNullValue(),
                                     base::FundamentalValue(false));
  }
}

void MediaRouterWebUIMessageHandler::UpdateIssue(const Issue* issue) {
  DVLOG(2) << "UpdateIssue";
  web_ui()->CallJavascriptFunction(kSetIssue,
      issue ? *IssueToValue(*issue) : *base::Value::CreateNullValue());
}

void MediaRouterWebUIMessageHandler::UpdateMaxDialogHeight(int height) {
  DVLOG(2) << "UpdateMaxDialogHeight";
  web_ui()->CallJavascriptFunction(kUpdateMaxHeight,
                                   base::FundamentalValue(height));
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
      kAcknowledgeFirstRunFlow,
      base::Bind(&MediaRouterWebUIMessageHandler::OnAcknowledgeFirstRunFlow,
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
      kJoinRoute,
      base::Bind(&MediaRouterWebUIMessageHandler::OnJoinRoute,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kCloseDialog,
      base::Bind(&MediaRouterWebUIMessageHandler::OnCloseDialog,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kReportClickedSinkIndex,
      base::Bind(&MediaRouterWebUIMessageHandler::OnReportClickedSinkIndex,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kReportInitialState,
      base::Bind(&MediaRouterWebUIMessageHandler::OnReportInitialState,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kReportInitialAction,
      base::Bind(&MediaRouterWebUIMessageHandler::OnReportInitialAction,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kReportRouteCreation,
      base::Bind(&MediaRouterWebUIMessageHandler::OnReportRouteCreation,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kReportSelectedCastMode,
      base::Bind(&MediaRouterWebUIMessageHandler::OnReportSelectedCastMode,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kReportNavigateToView,
      base::Bind(&MediaRouterWebUIMessageHandler::OnReportNavigateToView,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kReportSinkCount,
      base::Bind(&MediaRouterWebUIMessageHandler::OnReportSinkCount,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kReportTimeToClickSink,
      base::Bind(&MediaRouterWebUIMessageHandler::OnReportTimeToClickSink,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kReportTimeToInitialActionClose,
      base::Bind(
          &MediaRouterWebUIMessageHandler::OnReportTimeToInitialActionClose,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kOnInitialDataReceived,
      base::Bind(&MediaRouterWebUIMessageHandler::OnInitialDataReceived,
                 base::Unretained(this)));
}

void MediaRouterWebUIMessageHandler::OnRequestInitialData(
    const base::ListValue* args) {
  DVLOG(1) << "OnRequestInitialData";
  media_router_ui_->OnUIInitiallyLoaded();
  base::DictionaryValue initial_data;

  // "No Cast devices found?" Chromecast help center page.
  initial_data.SetString("deviceMissingUrl",
      base::StringPrintf(kHelpPageUrlPrefix, 3249268));

  scoped_ptr<base::ListValue> sinks(SinksToValue(media_router_ui_->sinks()));
  initial_data.Set("sinks", sinks.release());

  scoped_ptr<base::ListValue> routes(RoutesToValue(media_router_ui_->routes(),
      media_router_ui_->joinable_route_ids(),
      media_router_ui_->GetRouteProviderExtensionId()));
  initial_data.Set("routes", routes.release());

  const std::set<MediaCastMode> cast_modes = media_router_ui_->cast_modes();
  scoped_ptr<base::ListValue> cast_modes_list(
      CastModesToValue(cast_modes,
                       media_router_ui_->GetPresentationRequestSourceName()));
  initial_data.Set("castModes", cast_modes_list.release());

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
    // TODO(imcheng): Record error condition with UMA.
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

void MediaRouterWebUIMessageHandler::OnAcknowledgeFirstRunFlow(
    const base::ListValue* args) {
  DVLOG(1) << "OnAcknowledgeFirstRunFlow";
  Profile::FromWebUI(web_ui())->GetPrefs()->SetBoolean(
      prefs::kMediaRouterFirstRunFlowAcknowledged, true);

#if defined(GOOGLE_CHROME_BUILD)
  bool enabled_cloud_services = false;
  // Do not set the relevant cloud services prefs if the user was not shown
  // the cloud services prompt.
  if (!args->GetBoolean(0, &enabled_cloud_services)) {
    DVLOG(1) << "User was not shown the enable cloud services prompt.";
    return;
  }

  Profile::FromWebUI(web_ui())->GetPrefs()->SetBoolean(
      prefs::kMediaRouterEnableCloudServices, enabled_cloud_services);
  Profile::FromWebUI(web_ui())->GetPrefs()->SetBoolean(
      prefs::kMediaRouterCloudServicesPrefSet, true);
#endif  // defined(GOOGLE_CHROME_BUILD)
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

void MediaRouterWebUIMessageHandler::OnJoinRoute(const base::ListValue* args) {
  DVLOG(1) << "OnJoinRoute";
  const base::DictionaryValue* args_dict = nullptr;
  std::string route_id;
  std::string sink_id;
  if (!args->GetDictionary(0, &args_dict) ||
      !args_dict->GetString("sinkId", &sink_id) ||
      !args_dict->GetString("routeId", &route_id)) {
    DVLOG(1) << "Unable to extract args.";
    return;
  }

  if (sink_id.empty()) {
    DVLOG(1) << "Media Route UI did not respond with a "
             << "valid sink ID. Aborting.";
    return;
  }

  if (route_id.empty()) {
    DVLOG(1) << "Media Route UI did not respond with a "
             << "valid route ID. Aborting.";
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

  if (!media_router_ui_->ConnectRoute(sink_id, route_id)) {
    // TODO(boetger): Need to add an issue if failed to initiate a JoinRoute
    // request.
    DVLOG(1) << "Error initiating route join request.";
  }
}

void MediaRouterWebUIMessageHandler::OnCloseRoute(const base::ListValue* args) {
  DVLOG(1) << "OnCloseRoute";
  const base::DictionaryValue* args_dict = nullptr;
  std::string route_id;
  bool is_local = false;
  if (!args->GetDictionary(0, &args_dict) ||
      !args_dict->GetString("routeId", &route_id) ||
      !args_dict->GetBoolean("isLocal", &is_local)) {
    DVLOG(1) << "Unable to extract args.";
    return;
  }
  media_router_ui_->CloseRoute(route_id);
  UMA_HISTOGRAM_BOOLEAN("MediaRouter.Ui.Action.StopRoute", !is_local);
}

void MediaRouterWebUIMessageHandler::OnCloseDialog(
    const base::ListValue* args) {
  DVLOG(1) << "OnCloseDialog";
  if (dialog_closing_)
    return;

  dialog_closing_ = true;
  media_router_ui_->Close();
}

void MediaRouterWebUIMessageHandler::OnReportClickedSinkIndex(
    const base::ListValue* args) {
  DVLOG(1) << "OnReportClickedSinkIndex";
  int index;
  if (!args->GetInteger(0, &index)) {
    DVLOG(1) << "Unable to extract args.";
    return;
  }
  UMA_HISTOGRAM_SPARSE_SLOWLY("MediaRouter.Ui.Action.StartLocalPosition",
                              std::min(index, 100));
}

void MediaRouterWebUIMessageHandler::OnReportInitialAction(
  const base::ListValue* args) {
  DVLOG(1) << "OnReportInitialAction";
  int action;
  if (!args->GetInteger(0, &action)) {
    DVLOG(1) << "Unable to extract args.";
    return;
  }
  media_router::MediaRouterMetrics::RecordMediaRouterInitialUserAction(
      static_cast<MediaRouterUserAction>(action));
}

void MediaRouterWebUIMessageHandler::OnReportInitialState(
    const base::ListValue* args) {
  DVLOG(1) << "OnReportInitialState";
  std::string initial_view;
  if (!args->GetString(0, &initial_view)) {
    DVLOG(1) << "Unable to extract args.";
    return;
  }
  bool sink_list_state = initial_view == "sink-list";
  DCHECK(sink_list_state || (initial_view == "route-details"));
  UMA_HISTOGRAM_BOOLEAN("MediaRouter.Ui.InitialState", sink_list_state);
}

void MediaRouterWebUIMessageHandler::OnReportNavigateToView(
  const base::ListValue* args) {
  DVLOG(1) << "OnReportNavigateToView";
  std::string view;
  if (!args->GetString(0, &view)) {
    DVLOG(1) << "Unable to extract args.";
    return;
  }

  if (view == "cast-mode-list") {
    base::RecordAction(base::UserMetricsAction(
        "MediaRouter_Ui_Navigate_SinkListToSource"));
  } else if (view == "route-details") {
    base::RecordAction(base::UserMetricsAction(
        "MediaRouter_Ui_Navigate_SinkListToRouteDetails"));
  } else if (view == "sink-list") {
    base::RecordAction(base::UserMetricsAction(
        "MediaRouter_Ui_Navigate_RouteDetailsToSinkList"));
  }
}

void MediaRouterWebUIMessageHandler::OnReportRouteCreation(
    const base::ListValue* args) {
  DVLOG(1) << "OnReportRouteCreation";
  bool route_created_successfully;
  if (!args->GetBoolean(0, &route_created_successfully)) {
    DVLOG(1) << "Unable to extract args.";
    return;
  }

  UMA_HISTOGRAM_BOOLEAN("MediaRouter.Ui.Action.StartLocalSessionSuccessful",
                        route_created_successfully);
}

void MediaRouterWebUIMessageHandler::OnReportSelectedCastMode(
    const base::ListValue* args) {
  DVLOG(1) << "OnReportSelectedCastMode";
  int cast_mode_type;
  if (!args->GetInteger(0, &cast_mode_type)) {
    DVLOG(1) << "Unable to extract args.";
    return;
  }
  UMA_HISTOGRAM_SPARSE_SLOWLY("MediaRouter.Ui.Navigate.SourceSelection",
                              cast_mode_type);
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

void MediaRouterWebUIMessageHandler::OnReportTimeToClickSink(
    const base::ListValue* args) {
  DVLOG(1) << "OnReportTimeToClickSink";
  double time_to_click;
  if (!args->GetDouble(0, &time_to_click)) {
    DVLOG(1) << "Unable to extract args.";
    return;
  }
  UMA_HISTOGRAM_TIMES("MediaRouter.Ui.Action.StartLocal.Latency",
                      base::TimeDelta::FromMillisecondsD(time_to_click));
}

void MediaRouterWebUIMessageHandler::OnReportTimeToInitialActionClose(
    const base::ListValue* args) {
  DVLOG(1) << "OnReportTimeToInitialActionClose";
  double time_to_close;
  if (!args->GetDouble(0, &time_to_close)) {
    VLOG(0) << "Unable to extract args.";
    return;
  }
  UMA_HISTOGRAM_TIMES("MediaRouter.Ui.Action.CloseLatency",
                      base::TimeDelta::FromMillisecondsD(time_to_close));
}

void MediaRouterWebUIMessageHandler::OnInitialDataReceived(
    const base::ListValue* args) {
  DVLOG(1) << "OnInitialDataReceived";
  media_router_ui_->OnUIInitialDataReceived();
  MaybeUpdateFirstRunFlowData();
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

void MediaRouterWebUIMessageHandler::MaybeUpdateFirstRunFlowData() {
  base::DictionaryValue first_run_flow_data;

  Profile* profile = Profile::FromWebUI(web_ui());
  PrefService* pref_service = profile->GetPrefs();

  bool first_run_flow_acknowledged =
      pref_service->GetBoolean(prefs::kMediaRouterFirstRunFlowAcknowledged);
  bool show_cloud_pref = false;
#if defined(GOOGLE_CHROME_BUILD)
  // Cloud services preference is shown if user is logged in and has sync
  // enabled. If the user enables sync after acknowledging the first run flow,
  // this is treated as the user opting into Google services, including cloud
  // services, if the browser is a Chrome branded build.
  if (!pref_service->GetBoolean(prefs::kMediaRouterCloudServicesPrefSet) &&
      profile->IsSyncAllowed()) {
    SigninManagerBase* signin_manager =
        SigninManagerFactory::GetForProfile(profile);
    if (signin_manager && signin_manager->IsAuthenticated() &&
        ProfileSyncServiceFactory::GetForProfile(profile)->IsSyncActive()) {
      // If the user had previously acknowledged the first run flow without
      // being shown the cloud services option, and is now logged in with sync
      // enabled, turn on cloud services.
      if (first_run_flow_acknowledged) {
        pref_service->SetBoolean(prefs::kMediaRouterEnableCloudServices, true);
        pref_service->SetBoolean(prefs::kMediaRouterCloudServicesPrefSet,
                                 true);
        // Return early since the first run flow won't be surfaced.
        return;
      }

      show_cloud_pref = true;
      // "Casting to a Hangout from Chrome" Chromecast help center page.
      first_run_flow_data.SetString("firstRunFlowCloudPrefLearnMoreUrl",
          base::StringPrintf(kHelpPageUrlPrefix, 6320939));
    }
  }
#endif  // defined(GOOGLE_CHROME_BUILD)

  // Return early if the first run flow won't be surfaced.
  if (first_run_flow_acknowledged && !show_cloud_pref)
    return;

  // General Chromecast learn more page.
  first_run_flow_data.SetString("firstRunFlowLearnMoreUrl",
                                kCastLearnMorePageUrl);
  first_run_flow_data.SetBoolean("wasFirstRunFlowAcknowledged",
                                 first_run_flow_acknowledged);
  first_run_flow_data.SetBoolean("showFirstRunFlowCloudPref", show_cloud_pref);
  web_ui()->CallJavascriptFunction(kSetFirstRunFlowData, first_run_flow_data);
}

void MediaRouterWebUIMessageHandler::SetWebUIForTest(content::WebUI* web_ui) {
  set_web_ui(web_ui);
}

}  // namespace media_router
